// src/Database.cpp
#include "Database.h"
#include <filesystem>
#include <iostream>

namespace anemo {

Database::Database(const std::string& path)
  : db_(nullptr), path_(path) {}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

bool Database::open() {
    std::cout << "Opening database file '" << path_ << "'\n";
    return sqlite3_open(path_.c_str(), &db_) == SQLITE_OK;
}

    bool Database::beginTransaction() {
    char* err = nullptr;
    if (sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::commitTransaction() {
    char* err = nullptr;
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::rollbackTransaction() {
    char* err = nullptr;
    if (sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
}

    bool Database::initSchema() {
    const char* sql = R"(
    PRAGMA foreign_keys = ON;

    CREATE TABLE IF NOT EXISTS packages (
      name           TEXT PRIMARY KEY,
      version        TEXT NOT NULL,
      arch           TEXT NOT NULL,
      install_script TEXT
    );

    -- Note: dependency column has no foreign key constraint
    -- so we can delete the referenced package row freely.
    CREATE TABLE IF NOT EXISTS dependencies (
      package    TEXT NOT NULL,
      dependency TEXT NOT NULL,
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
    bool ok = sqlite3_exec(db_, sql, nullptr, nullptr, &err) == SQLITE_OK;
    if (!ok) {
        std::cerr << "DB schema error: " << err << "\n";
        sqlite3_free(err);
    }
    return ok;
}

bool Database::addPackage(const Package::Metadata& meta,
                          const std::string& installScriptPath)
{
    // 1) Insert/replace into packages
    const char* sql_pkg =
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
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (!ok) return false;

    // 2) Refresh dependencies
    const char* sql_del = "DELETE FROM dependencies WHERE package = ?;";
    if (sqlite3_prepare_v2(db_, sql_del, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt,1,meta.name.c_str(),-1,nullptr);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    const char* sql_ins =
      "INSERT INTO dependencies(package,dependency) VALUES(?,?);";
    for (auto& d : meta.deps) {
        if (sqlite3_prepare_v2(db_, sql_ins, -1, &stmt, nullptr) != SQLITE_OK)
            continue;
        sqlite3_bind_text(stmt,1,meta.name.c_str(),-1,nullptr);
        sqlite3_bind_text(stmt,2,d.c_str(),-1,nullptr);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    return true;
}

std::vector<std::string> Database::getReverseDependencies(const std::string& pkg) {
    std::vector<std::string> result;
    const char* sql =
      "SELECT package FROM dependencies WHERE dependency = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt,1,pkg.c_str(),-1,nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
            if (txt) result.emplace_back(txt);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

std::vector<std::string> Database::getFiles(const std::string& pkg) {
    std::vector<std::string> result;
    const char* sql =
      "SELECT filepath FROM files WHERE package = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt,1,pkg.c_str(),-1,nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
            if (txt) result.emplace_back(txt);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

std::string Database::getInstallScript(const std::string& pkg) {
    std::string result;
    const char* sql =
      "SELECT install_script FROM packages WHERE name = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt,1,pkg.c_str(),-1,nullptr);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            auto txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
            if (txt) result = txt;
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

bool Database::removeFiles(const std::string& pkg) {
    const char* sql = "DELETE FROM files WHERE package = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt,1,pkg.c_str(),-1,nullptr);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::deletePackage(const std::string& pkg) {
    const char* sql = "DELETE FROM packages WHERE name = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt,1,pkg.c_str(),-1,nullptr);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::markBroken(const std::string& pkg) {
    const char* sql =
      "INSERT OR IGNORE INTO broken_packages(name) VALUES(?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt,1,pkg.c_str(),-1,nullptr);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

    bool Database::isInstalled(const std::string& name, const std::string& version) const {
    const char* sql = "SELECT COUNT(1) FROM packages WHERE name = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, nullptr);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) found = sqlite3_column_int(stmt, 0) > 0;
    sqlite3_finalize(stmt);
    return found;
}

    bool Database::logFile(const std::string& pkgName, const std::string& filePath) {
        // Note: our table columns are "package" and "filepath"
        const char* sql =
          "INSERT INTO files(package, filepath) VALUES(?,?);";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "\033[31mDB error:\033[0m failed to prepare logFile statement: "
                      << sqlite3_errmsg(db_) << "\n";
            return false;
        }

        sqlite3_bind_text(stmt, 1, pkgName.c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, filePath.c_str(), -1, SQLITE_TRANSIENT);

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
    const char* sql = "SELECT name FROM broken_packages;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (txt) result.emplace_back(txt);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

    std::vector<std::string> Database::getDependencies(const std::string& packageName) const {
    std::vector<std::string> result;
    const char* sql =
      "SELECT dependency FROM dependencies WHERE package = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, packageName.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (txt) result.emplace_back(txt);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

    bool Database::removeBroken(const std::string& packageName) {
    const char* sql = "DELETE FROM broken_packages WHERE name = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DB error: failed to prepare removeBroken: "
                  << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, packageName.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) {
        std::cerr << "DB error: failed to delete broken_packages entry: "
                  << sqlite3_errmsg(db_) << "\n";
    }
    sqlite3_finalize(stmt);
    return ok;
}

    std::vector<PackageInfo> Database::listPackages() const {
    std::vector<PackageInfo> out;
    const char* sql = R"(
      SELECT p.name, p.version, p.arch,
             (b.name IS NOT NULL) AS broken
        FROM packages p
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

} // namespace anemo
