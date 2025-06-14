//
// Created by cv2 on 6/12/25.
//

#ifndef INSTALLER_H
#define INSTALLER_H

#include <string>
#include <unordered_set>

#include "Database.h"
#include "Repository.h"
#include "DependencyResolver.h"
#include <regex>

namespace anemo {

    class Installer {
    public:
        Installer(Database& db,
                  Repository& repo,
                  bool force = false,
                  const std::string& rootDir = "/",
                  const std::unordered_set<std::string>& staged = {});

        // Install a standalone .apkg archive
        bool installArchive(const std::string& archivePath);

        // Repo-based operations
        bool installPackage(const std::string& name, const std::string& version);
        bool removePackage(const std::string& name);
        bool updatePackage(const std::string& name);

    private:
        // Core dependencies
        Database& db_;
        Repository& repo_;
        DependencyResolver resolver_;

        // Installation options
        bool force_;
        std::string rootDir_;

        // Internal state
        bool warnings_;

        // Helpers
        std::string detectHostArch();
        std::string makeTempDir();
        std::unordered_set<std::string> staged_;
    };

} // namespace anemo


#endif //INSTALLER_H
