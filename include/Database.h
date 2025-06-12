//
// Created by cv2 on 6/12/25.
//

#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include "Package.h"

namespace anemo {

    class Database {
    public:
        Database(const std::string& dbPath);
        ~Database();

        bool open();
        void close();
        bool initSchema();

        bool beginTransaction();
        bool commitTransaction();
        bool rollbackTransaction();

        bool addPackage(const Package::Metadata& meta, const std::string& installScriptPath);
        bool removePackage(const std::string& name);
        bool logFile(const std::string& pkgName, const std::string& filePath);
        bool markBroken(const std::string& pkgName);

        [[nodiscard]] std::vector<Package::Metadata> installedPackages() const;
        bool isInstalled(const std::string& name, const std::string& version) const;

    private:
        std::string dbPath_;
        sqlite3* db_ = nullptr;
    };

} // namespace anemo

#endif //DATABASE_H
