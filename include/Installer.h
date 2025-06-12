//
// Created by cv2 on 6/12/25.
//

#ifndef INSTALLER_H
#define INSTALLER_H

#include <string>
#include "Database.h"
#include "Repository.h"
#include "DependencyResolver.h"

namespace anemo {

    class Installer {
    public:
        Installer(Database& db, Repository& repo);

        bool installPackage(const std::string& name, const std::string& version);
        bool removePackage(const std::string& name);
        bool updatePackage(const std::string& name);

    private:
        Database& db_;
        Repository& repo_;
        DependencyResolver resolver_;
    };

} // namespace anemo


#endif //INSTALLER_H
