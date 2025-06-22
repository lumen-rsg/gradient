//
// Created by cv2 on 6/12/25.
//

#ifndef PACKAGE_H
#define PACKAGE_H

#include <string>
#include <vector>

namespace gradient {
    class Package {
    public:
        struct Metadata {
            std::string name, version, arch, description;
            std::vector<std::string> deps, makedepends, conflicts, replaces, provides;
        };
        explicit Package(const std::string& archivePath);
        bool loadMetadata();
        [[nodiscard]] const Metadata& metadata() const;
    private:
        std::string archivePath_;
        Metadata meta_;
    };
} // namespace anemo

#endif //PACKAGE_H
