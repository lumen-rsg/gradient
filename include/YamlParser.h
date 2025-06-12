//
// Created by cv2 on 6/12/25.
//

#ifndef YAMLPARSER_H
#define YAMLPARSER_H

#include <string>
#include "Package.h"

namespace anemo {

    class YamlParser {
    public:
        static bool parseMetadata(const std::string& yamlPath, Package::Metadata& outMeta);
    };

} // namespace anemo

#endif //YAMLPARSER_H
