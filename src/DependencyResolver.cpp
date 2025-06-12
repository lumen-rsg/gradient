//
// Created by cv2 on 6/12/25.
//

#include "DependencyResolver.h"
#include <stdexcept>

#include "Database.h"
#include "Repository.h"

namespace anemo {

    DependencyResolver::DependencyResolver(Database& db, Repository& repo)
        : db_(db), repo_(repo) {}

    std::vector<std::string> DependencyResolver::resolveInstall(const Package::Metadata& target) {
        std::vector<std::string> order;
        visited_.clear();
        resolveRecursive(target.name, order);
        // add target at end
        order.push_back(target.name + "-" + target.version + ".apkg");
        return order;
    }

    void DependencyResolver::resolveRecursive(const std::string& pkgName, std::vector<std::string>& order) {
        if (std::find(visited_.begin(), visited_.end(), pkgName) != visited_.end()) return;
        visited_.push_back(pkgName);

        // fetch metadata from repo (latest version)
        auto pkg = repo_.fetchPackage(pkgName, "");
        if (!pkg || !pkg->loadMetadata())
            throw std::runtime_error("Failed to fetch metadata for " + pkgName);

        for (const auto& dep : pkg->metadata().deps) {
            if (!db_.isInstalled(dep, "")) {
                resolveRecursive(dep, order);
            }
        }
        order.push_back(pkgName + "-" + pkg->metadata().version + ".apkg");
    }

} // namespace anemo