//
// Created by cv2 on 6/12/25.
//

#include "Installer.h"
#include "TarHandler.h"
#include "YamlParser.h"
#include "ScriptExecutor.h"
#include <iostream>

namespace anemo {

    Installer::Installer(Database& db, Repository& repo)
        : db_(db), repo_(repo), resolver_(db, repo) {}

    bool Installer::installPackage(const std::string& name, const std::string& version) {
        // fetch target metadata
        auto pkgPtr = repo_.fetchPackage(name, version);
        if (!pkgPtr || !pkgPtr->loadMetadata()) return false;
        auto meta = pkgPtr->metadata();

        // resolve full install order
        auto archives = resolver_.resolveInstall(meta);
        for (const auto& archive : archives) {
            // extract files
            if (!TarHandler::extract(archive, "/")) return false;
            // run post-install script
            ScriptExecutor::runScript(archive + "/install.anemonix", "post_install");
            // add to DB
            if (!db_.addPackage(meta)) return false;
        }
        return true;
    }

    bool Installer::removePackage(const std::string& name) {
        // TODO: resolve reverse dependencies
        std::string script = "/var/lib/anemo/packages/" + name + "/install.anemonix";
        ScriptExecutor::runScript(script, "pre_remove");
        // remove files (TODO)
        db_.removePackage(name);
        ScriptExecutor::runScript(script, "post_remove");
        return true;
    }

    bool Installer::updatePackage(const std::string& name) {
        // TODO: fetch latest version and call install
        return installPackage(name, "");
    }

} // namespace anemo