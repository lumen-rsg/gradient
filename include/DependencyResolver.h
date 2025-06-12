//
// Created by cv2 on 6/12/25.
//

#ifndef DEPENDENCYRESOLVER_H
#define DEPENDENCYRESOLVER_H

#include <vector>
#include <string>
#include "Package.h"
#include "Database.h"
#include "Repository.h"

namespace anemo {

    class DependencyResolver {
    public:
        DependencyResolver(Database& db, Repository& repo);

        // Determine install order including dependencies
        // Returns ordered list of package archives to install
        std::vector<std::string> resolveInstall(const Package::Metadata& target);

    private:
        Database& db_;
        Repository& repo_;
        std::vector<std::string> visited_;

        void resolveRecursive(const std::string& pkgName, std::vector<std::string>& order);
    };

} // namespace anemo

#endif //DEPENDENCYRESOLVER_H
