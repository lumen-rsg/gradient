// src/Database.cpp
#include "Database.h"
#include <filesystem>
#include <iostream>
#include <utility>

#include "tools.h"

namespace gradient {

    Database::Database(std::string path)
      : db_(nullptr), path_(std::move(path)) {}

    Database::~Database() {
        if (db_) sqlite3_close(db_);
    }

    bool Database::open() {
        return sqlite3_open(path_.c_str(), &db_) == SQLITE_OK;
    }
    bool Database::beginTransaction() const {
        char* err = nullptr;
        if (sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, &err) != SQLITE_OK) {
            sqlite3_free(err);
            return false;
        }
        return true;
    }

    bool Database::commitTransaction() const {
        char* err = nullptr;
        if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
            sqlite3_free(err);
            return false;
        }
        return true;
    }

    bool Database::getPackageVersion(const std::string& pkg, std::string& out) const {
        sqlite3_stmt* stmt = nullptr;
        if (const auto sql = "SELECT version FROM packages WHERE name = ?;";
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text(stmt,1,pkg.c_str(),-1,SQLITE_TRANSIENT);
        bool ok = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (const auto txt = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))) {
                out = txt;
                ok = true;
            }
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    bool Database::rollbackTransaction() const {
        char* err = nullptr;
        if (sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &err) != SQLITE_OK) {
            sqlite3_free(err);
            return false;
        }
        return true;
    }

    bool Database::initSchema() const {
        const auto sql = R"(
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS packages (
          name           TEXT PRIMARY KEY,
          version        TEXT NOT NULL,
          arch           TEXT NOT NULL,
          install_script TEXT
        );

        CREATE TABLE IF NOT EXISTS dependencies (
          package    TEXT NOT NULL,
          dependency TEXT NOT NULL,
          FOREIGN KEY(package) REFERENCES packages(name) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS provides (
          package  TEXT NOT NULL,
          provided TEXT NOT NULL,
          FOREIGN KEY(package) REFERENCES packages(name) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS files (
          package  TEXT NOT NULL,
          filepath TEXT NOT NULL,
          FOREIGN KEY(package) REFERENCES packages(name) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS broken_packages (
          name TEXT PRIMARY KEY
        );
        )";
        char* err = nullptr;
        const bool ok = sqlite3_exec(db_, sql, nullptr, nullptr, &err) == SQLITE_OK;
        if (!ok) {
            std::cerr << "DB schema error: " << err << "\n";
            sqlite3_free(err);
        }
        return ok;
    }

    bool Database::addPackage(const Package::Metadata& meta,
                              const std::string& installScriptPath) const {
        // 1) Insert/replace into packages
        const auto sql_pkg =
          "INSERT OR REPLACE INTO packages(name,version,arch,install_script) "
          "VALUES(?,?,?,?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql_pkg, -1, &stmt, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text(stmt,1,meta.name.c_str(),-1,nullptr);
        sqlite3_bind_text(stmt,2,meta.version.c_str(),-1,nullptr);
        sqlite3_bind_text(stmt,3,meta.arch.c_str(),-1,nullptr);
        if (installScriptPath.empty())
            sqlite3_bind_null(stmt,4);
        else
            sqlite3_bind_text(stmt,4,installScriptPath.c_str(),-1,nullptr);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (!ok) return false;

        // 2) Refresh dependencies
        if (const auto sql_del = "DELETE FROM dependencies WHERE package = ?;";
            sqlite3_prepare_v2(db_, sql_del, -1, &stmt, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text(stmt,1,meta.name.c_str(),-1,nullptr);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        for (auto& d : meta.deps) {
            const auto sql_ins =
                    "INSERT INTO dependencies(package,dependency) VALUES(?,?);";
            if (sqlite3_prepare_v2(db_, sql_ins, -1, &stmt, nullptr) != SQLITE_OK)
                continue;
            sqlite3_bind_text(stmt,1,meta.name.c_str(),-1,nullptr);
            sqlite3_bind_text(stmt,2,d.c_str(),-1,nullptr);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        if (!addProvides(meta)) return false;
        return true;
    }

    bool Database::addProvides(const Package::Metadata& meta) const {
        sqlite3_stmt* stmt = nullptr;
        // 1) delete old provides for this package
        if (sqlite3_prepare_v2(db_,
              "DELETE FROM provides WHERE package = ?;", -1, &stmt, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text(stmt, 1, meta.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // 2) insert each provided name
        for (auto const& prov : meta.provides) {
            if (const auto sql = "INSERT INTO provides(package, provided) VALUES(?,?);";
                sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
                return false;
            sqlite3_bind_text(stmt, 1, meta.name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, prov.c_str(),       -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                sqlite3_finalize(stmt);
                return false;
            }
            sqlite3_finalize(stmt);
        }
        return true;
    }

    bool Database::isProvided(const std::string& name) const {
    sqlite3_stmt* stmt = nullptr;
    bool result = false;
    if (sqlite3_prepare_v2(db_,
          "SELECT 1 FROM provides WHERE provided = ? LIMIT 1;", -1, &stmt, nullptr)
        == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = true;
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

    std::vector<std::string> Database::getReverseDependencies(const std::string& packageName) const {
        std::vector<std::string> result;
        const auto sql =
          "SELECT package FROM dependencies WHERE dependency = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt,1,packageName.c_str(),-1,nullptr);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (auto txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0))) result.emplace_back(txt);
            }
            sqlite3_finalize(stmt);
        }
        return result;
    }

    std::vector<std::string> Database::getFiles(const std::string& packageName) const {
        std::vector<std::string> result;
        const auto sql =
          "SELECT filepath FROM files WHERE package = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt,1,packageName.c_str(),-1,nullptr);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (auto txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0))) result.emplace_back(txt);
            }
            sqlite3_finalize(stmt);
        }
        return result;
    }

    std::string Database::getInstallScript(const std::string& packageName) const {
        std::string result;
        const auto sql =
          "SELECT install_script FROM packages WHERE name = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt,1,packageName.c_str(),-1,nullptr);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                if (const auto txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0))) result = txt;
            }
            sqlite3_finalize(stmt);
        }
        return result;
    }

    bool Database::removeFiles(const std::string& packageName) const {
        sqlite3_stmt* stmt = nullptr;

        if (const auto sql = "DELETE FROM files WHERE package = ?;";
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return false;

        sqlite3_bind_text(stmt,1,packageName.c_str(),-1,nullptr);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);

        return ok;
    }

    bool Database::deletePackage(const std::string& packageName) const {
        sqlite3_stmt* stmt = nullptr;

        if (const auto sql = "DELETE FROM packages WHERE name = ?;";
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return false;

        sqlite3_bind_text(stmt,1,packageName.c_str(),-1,nullptr);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);

        return ok;
    }

    bool Database::markBroken(const std::string& packageName) const {
        const auto sql =
          "INSERT OR IGNORE INTO broken_packages(name) VALUES(?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text(stmt,1,packageName.c_str(),-1,nullptr);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool Database::isInstalled(const std::string& name, const std::string& version) const {
        sqlite3_stmt* stmt;
        if (const auto sql = "SELECT COUNT(1) FROM packages WHERE name = ?";
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, name.c_str(), -1, nullptr);
        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) found = sqlite3_column_int(stmt, 0) > 0;
        sqlite3_finalize(stmt);
        return found;
    }

    bool Database::logFile(const std::string& pkg, const std::string& path) const {
        // Note: our table columns are "package" and "filepath"
        const auto sql =
          "INSERT INTO files(package, filepath) VALUES(?,?);";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "\033[31mDB error:\033[0m failed to prepare logFile statement: "
                      << sqlite3_errmsg(db_) << "\n";
            return false;
        }

        sqlite3_bind_text(stmt, 1, pkg.c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "\033[31mDB error:\033[0m failed to execute logFile INSERT: "
                      << sqlite3_errmsg(db_) << "\n";
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

    std::vector<std::string> Database::getBrokenPackages() const {
        std::vector<std::string> result;
        sqlite3_stmt* stmt = nullptr;
        if (const auto sql = "SELECT name FROM broken_packages;";
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (auto txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) result.emplace_back(txt);
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

    std::vector<std::string> Database::getDependencies(const std::string& packageName) const {
        std::vector<std::string> result;
        const auto sql =
          "SELECT dependency FROM dependencies WHERE package = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, packageName.c_str(), -1, SQLITE_TRANSIENT);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (auto txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) result.emplace_back(txt);
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

    bool Database::removeBroken(const std::string& packageName) const {
        sqlite3_stmt* stmt = nullptr;
        if (const auto sql = "DELETE FROM broken_packages WHERE name = ?;";
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "DB error: failed to prepare removeBroken: "
                      << sqlite3_errmsg(db_) << "\n";
            return false;
        }
        sqlite3_bind_text(stmt, 1, packageName.c_str(), -1, SQLITE_TRANSIENT);
        const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
        if (!ok) {
            std::cerr << "DB error: failed to delete broken_packages entry: "
                      << sqlite3_errmsg(db_) << "\n";
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    std::vector<PackageInfo> Database::listPackages() const {
        std::vector<PackageInfo> out;
        const auto sql = R"(
        SELECT p.name, p.version, p.arch, (b.name IS NOT NULL) AS broken FROM packages p
        LEFT JOIN broken_packages b
              ON p.name = b.name
        ORDER BY p.name;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "DB error: listPackages prepare failed: "
                      << sqlite3_errmsg(db_) << "\n";
            return out;
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            PackageInfo pi;
            pi.name    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            pi.version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            pi.arch    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            pi.broken  = sqlite3_column_int(stmt, 3) != 0;
            out.push_back(std::move(pi));
        }
        sqlite3_finalize(stmt);
        return out;
    }

    bool Database::providesSatisfies(const Tools::Constraint& c) const {
        // find any row whose "provided" raw string starts with c.name
        // e.g. "sdl2" matches "sdl2=2.32.56"
        const auto sql =
          "SELECT provided FROM provides WHERE provided LIKE ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "DB error: providesSatisfies prepare failed: "
                      << sqlite3_errmsg(db_) << "\n";
            return false;
        }

        // bind "sdl2%" to catch both "sdl2" and "sdl2=1.2"
        const std::string like = c.name + "%";
        sqlite3_bind_text(stmt, 1, like.c_str(), -1, SQLITE_TRANSIENT);

        bool ok = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto txt = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            if (!txt) continue;
            std::string rawProv(txt);
            // parse it, e.g. rawProv="sdl2=2.32.56" → pc.name="sdl2", pc.op="=", pc.ver="2.32.56"
            Tools::Constraint pc = Tools::parseConstraint(rawProv);
            if (pc.name != c.name)
                continue;   // e.g. "sdl23" won't match "sdl2"
            // if no op on the dependency, any provider works
            if (c.op.empty() || Tools::evalConstraint(pc.version, c)) {
                ok = true;
                break;
            }
        }
        sqlite3_finalize(stmt);
        return ok;
    }

} // namespace anemo
