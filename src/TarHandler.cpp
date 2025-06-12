//
// Created by cv2 on 6/12/25.
//

#include "TarHandler.h"
#include <cstdlib>
namespace anemo {

    bool TarHandler::extract(const std::string& archive, const std::string& dest) {
        std::string cmd = "tar -xf " + archive + " -C " + dest;
        return std::system(cmd.c_str()) == 0;
    }

    bool TarHandler::extractMember(const std::string& archive,
                                   const std::string& member,
                                   const std::string& destDir) {
        std::string cmd = "tar -xf " + archive +
                          " " + member +
                          " -C " + destDir;
        return std::system(cmd.c_str()) == 0;
    }

} // namespace anemo
