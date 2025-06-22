//
// Created by cv2 on 6/12/25.
//

#ifndef TARHANDLER_H
#define TARHANDLER_H

#include <string>
namespace gradient {
    class TarHandler {
    public:
        static bool extract(const std::string& archive, const std::string& dest);
        static bool create(const std::string& sourceDir, const std::string& archive);
        static bool extractMember(const std::string& archive, const std::string& member, const std::string& destDir);
    };
} // namespace anemo

#endif //TARHANDLER_H
