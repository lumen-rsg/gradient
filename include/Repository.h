//
// Created by cv2 on 6/12/25.
//

#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <string>
#include <vector>
#include <memory>
#include "Package.h"
namespace anemo {
    class Repository {
    public:
        Repository(const std::string& url, const std::string& localPath);
        bool sync();
        [[nodiscard]] std::vector<Package::Metadata> listPackages() const;
        std::unique_ptr<Package> fetchPackage(const std::string& name, const std::string& version);
    private:
        std::string url_, localPath_;
    };
} // namespace anemo

#endif //REPOSITORY_H
