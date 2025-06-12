//
// Created by cv2 on 6/12/25.
//

#include "Database.h"
#include <stdexcept>

namespace anemo {

Database::Database(const std::string& dbPath):dbPath_(dbPath){}
Database::~Database(){ close(); }

bool Database::open() {
    return sqlite3_open(dbPath_.c_str(), &db_) == SQLITE_OK;
}

void Database::close() {
    if (db_) sqlite3_close(db_), db_ = nullptr;
}

bool Database::initSchema() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS packages ("
        " name TEXT PRIMARY KEY,"
        " version TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS files ("
        " pkg_name TEXT,"
        " file_path TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS broken_packages ("
        " pkg_name TEXT PRIMARY KEY"
        ");";
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
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

bool Database::addPackage(const Package::Metadata& meta) {
    const char* sql = "INSERT OR REPLACE INTO packages(name,version) VALUES(?,?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, meta.name.c_str(), -1, nullptr);
    sqlite3_bind_text(stmt, 2, meta.version.c_str(), -1, nullptr);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::removePackage(const std::string& name) {
    const char* sql1 = "DELETE FROM files WHERE pkg_name = ?;";
    sqlite3_stmt* stmt1;
    if (sqlite3_prepare_v2(db_, sql1, -1, &stmt1, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt1, 1, name.c_str(), -1, nullptr);
        sqlite3_step(stmt1);
        sqlite3_finalize(stmt1);
    }
    const char* sql2 = "DELETE FROM packages WHERE name = ?;";
    sqlite3_stmt* stmt2;
    if (sqlite3_prepare_v2(db_, sql2, -1, &stmt2, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt2, 1, name.c_str(), -1, nullptr);
    bool ok = sqlite3_step(stmt2) == SQLITE_DONE;
    sqlite3_finalize(stmt2);
    return ok;
}

bool Database::logFile(const std::string& pkgName, const std::string& filePath) {
    const char* sql = "INSERT INTO files(pkg_name, file_path) VALUES(?,?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, pkgName.c_str(), -1, nullptr);
    sqlite3_bind_text(stmt, 2, filePath.c_str(), -1, nullptr);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::markBroken(const std::string& pkgName) {
    const char* sql = "INSERT OR REPLACE INTO broken_packages(pkg_name) VALUES(?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, pkgName.c_str(), -1, nullptr);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Package::Metadata> Database::installedPackages() const {
    return {};
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

} // namespace anemo