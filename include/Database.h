// include/anemo/Database.h
#pragma once

#include "Package.h"
#include <string>
#include <vector>
#include <sqlite3.h>

#include "tools.h"


namespace gradient {

    // A little struct to hold what we need for `list`
    struct PackageInfo {
        std::string name;
        std::string version;
        std::string arch;
        bool broken;
    };


    class Database {
    public:
        Database(std::string  path);
        ~Database();

        bool open();
        bool initSchema() const;

        // Install
        bool addPackage(const Package::Metadata& meta,
                        const std::string& installScriptPath) const;
        bool addProvides(const Package::Metadata& meta) const;
        bool isProvided(const std::string& name) const;

        // Removal support
        std::vector<std::string> getReverseDependencies(const std::string& packageName) const;
        std::vector<std::string> getFiles(const std::string& packageName) const;
        std::string getInstallScript(const std::string& packageName) const;
        bool removeFiles(const std::string& packageName) const;
        bool deletePackage(const std::string& packageName) const;
        bool markBroken(const std::string& packageName) const;

        // Existing APIs
        bool isInstalled(const std::string& name, const std::string& version) const;
        bool logFile(const std::string& pkg, const std::string& path) const;
        bool beginTransaction() const;
        bool commitTransaction() const;

        bool getPackageVersion(const std::string &pkg, std::string &out) const;

        bool rollbackTransaction() const;

        // ** New for audit **
        /// List all packages currently marked broken
        [[nodiscard]] std::vector<std::string> getBrokenPackages() const;
        /// Fetch the runtime dependencies (the deps: list) of a given package
        [[nodiscard]] std::vector<std::string> getDependencies(const std::string& packageName) const;
        /// Remove a package from the broken_packages table
        bool removeBroken(const std::string& packageName) const;
        [[nodiscard]] std::vector<PackageInfo> listPackages() const;

        bool providesSatisfies(const Tools::Constraint &c) const;

    private:
        sqlite3* db_;
        std::string path_;
    };

} // namespace anemo
