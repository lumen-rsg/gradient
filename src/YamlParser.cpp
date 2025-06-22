// src/YamlParser.cpp
// Created by cv2 on 6/12/25.

#include "YamlParser.h"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace gradient {

bool YamlParser::parseMetadata(const std::string& path, Package::Metadata& meta) {
    // 1) Check file existence
    if (!fs::exists(path)) {
        std::cerr << "ERROR: metadata file not found at '" << path << "'\n";
        return false;
    }

    // 2) Read file contents
    std::ifstream in(path);
    if (!in) {
        std::cerr << "ERROR: failed to open '" << path << "' for reading\n";
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string content = buffer.str();

    // 3) Parse with yaml-cpp
    YAML::Node root;
    try {
        root = YAML::Load(content);
    } catch (const YAML::Exception& e) {
        std::cerr << "YAML PARSE EXCEPTION: " << e.what() << "\n";
        return false;
    }

    // 4) Extract fields
    try {
        meta.name        = root["name"].as<std::string>();
        meta.version     = root["version"].as<std::string>();
        meta.arch        = root["arch"].as<std::string>();

        auto readList = [&](const char* key, std::vector<std::string>& dst) {
            if (root[key] && root[key].IsSequence()) {
                for (const auto& node : root[key]) {
                    dst.push_back(node.as<std::string>());
                }
            }
        };

        readList("deps",         meta.deps);
        readList("makedepends",  meta.makedepends);
        readList("conflicts",    meta.conflicts);
        readList("replaces",     meta.replaces);
        readList("provides",     meta.provides);

        if (root["description"])
            meta.description = root["description"].as<std::string>();
    }
    catch (const YAML::Exception& e) {
        std::cerr << "YAML FIELD EXCEPTION: " << e.what() << "\n";
        return false;
    }

    return true;
}

} // namespace anemo
