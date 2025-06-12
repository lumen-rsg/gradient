//
// Created by cv2 on 6/12/25.
//

#include "YamlParser.h"
#include <yaml-cpp/yaml.h>
namespace anemo {
    bool YamlParser::parseMetadata(const std::string& yamlPath,Package::Metadata& outMeta){
        try{ auto node=YAML::LoadFile(yamlPath);
            outMeta.name=node["name"].as<std::string>();
            outMeta.version=node["version"].as<std::string>();
            outMeta.arch=node["arch"].as<std::string>();
            outMeta.description=node["description"].as<std::string>();
            outMeta.deps=node["deps"].as<std::vector<std::string>>();
            outMeta.makedepends=node["makedepends"].as<std::vector<std::string>>();
            outMeta.conflicts=node["conflicts"].as<std::vector<std::string>>();
            outMeta.replaces=node["replaces"].as<std::vector<std::string>>();
            outMeta.provides=node["provides"].as<std::vector<std::string>>();
            return true;} catch(...){return false;}
    }
}
