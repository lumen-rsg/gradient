//
// Created by cv2 on 6/12/25.
//

#include "Database.h"
#include <stdexcept>
namespace anemo {
    Database::Database(const std::string& dbPath):dbPath_(dbPath){}
    Database::~Database(){close();}
    bool Database::open(){ if(sqlite3_open(dbPath_.c_str(),&db_)!=SQLITE_OK)return false;return true;}
    void Database::close(){if(db_){sqlite3_close(db_);db_=nullptr;}}
    bool Database::initSchema(){const char* sql="CREATE TABLE IF NOT EXISTS packages(name TEXT,version TEXT,PRIMARY KEY(name));";char*err; if(sqlite3_exec(db_,sql,nullptr,nullptr,&err)!=SQLITE_OK){sqlite3_free(err);return false;}return true;}
    bool Database::addPackage(const Package::Metadata& meta){const char*sql="INSERT OR REPLACE INTO packages(name,version) VALUES(?,?);";sqlite3_stmt*stmt; if(sqlite3_prepare_v2(db_,sql,-1,&stmt,nullptr)!=SQLITE_OK)return false;sqlite3_bind_text(stmt,1,meta.name.c_str(),-1,nullptr);sqlite3_bind_text(stmt,2,meta.version.c_str(),-1,nullptr);bool ok=(sqlite3_step(stmt)==SQLITE_DONE);sqlite3_finalize(stmt);return ok;}
    bool Database::removePackage(const std::string& name){const char*sql="DELETE FROM packages WHERE name=?;";sqlite3_stmt*stmt; if(sqlite3_prepare_v2(db_,sql,-1,&stmt,nullptr)!=SQLITE_OK)return false;sqlite3_bind_text(stmt,1,name.c_str(),-1,nullptr);bool ok=(sqlite3_step(stmt)==SQLITE_DONE);sqlite3_finalize(stmt);return ok;}
    std::vector<Package::Metadata> Database::installedPackages() const{return {};}
    bool Database::isInstalled(const std::string& name,const std::string& version) const{return false;}
}