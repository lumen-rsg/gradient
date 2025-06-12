//
// Created by cv2 on 6/12/25.
//

#ifndef TARHANDLER_H
#define TARHANDLER_H

#include <string>
namespace anemo {
    class TarHandler {
    public:
        static bool extract(const std::string& archive, const std::string& dest);
        static bool create(const std::string& sourceDir, const std::string& archive);
    };
} // namespace anemo

#endif //TARHANDLER_H
