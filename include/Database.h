// include/anemo/Database.h
#pragma once

#include "Package.h"
#include <string>
#include <vector>
#include <sqlite3.h>

namespace anemo {

    class Database {
    public:
        Database(const std::string& path);
        ~Database();

        bool open();
        bool initSchema();

        // Install
        bool addPackage(const Package::Metadata& meta,
                        const std::string& installScriptPath);

        // Removal support
        std::vector<std::string> getReverseDependencies(const std::string& packageName);
        std::vector<std::string> getFiles(const std::string& packageName);
        std::string getInstallScript(const std::string& packageName);
        bool removeFiles(const std::string& packageName);
        bool deletePackage(const std::string& packageName);
        bool markBroken(const std::string& packageName);
        bool removeReverseDependencies(const std::string& pkgName);

        // Existing APIs
        bool isInstalled(const std::string& name, const std::string& version) const;
        bool logFile(const std::string& pkg, const std::string& path);
        bool beginTransaction();
        bool commitTransaction();
        bool rollbackTransaction();

        // ** New for audit **
        /// List all packages currently marked broken
        [[nodiscard]] std::vector<std::string> getBrokenPackages() const;
        /// Fetch the runtime dependencies (the deps: list) of a given package
        [[nodiscard]] std::vector<std::string> getDependencies(const std::string& packageName) const;
        /// Remove a package from the broken_packages table
        bool removeBroken(const std::string& packageName);

    private:
        sqlite3* db_;
        std::string path_;
    };

} // namespace anemo
