// src/YamlParser.cpp
// Created by cv2 on 6/12/25.

#include "YamlParser.h"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace anemo {

bool YamlParser::parseMetadata(const std::string& path, Package::Metadata& meta) {
    std::cerr << "DEBUG: YamlParser::parseMetadata('" << path << "')\n";

    // 1) Check file existence
    if (!fs::exists(path)) {
        std::cerr << "DEBUG ERROR: metadata file not found at '" << path << "'\n";
        return false;
    }

    // 2) Read file contents
    std::ifstream in(path);
    if (!in) {
        std::cerr << "DEBUG ERROR: failed to open '" << path << "' for reading\n";
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string content = buffer.str();

    std::cerr << "DEBUG: Raw YAML content (" << content.size() << " bytes):\n";
    std::cerr << "------ BEGIN YAML ------\n";
    std::cerr << content;
    std::cerr << "\n------- END YAML -------\n";

    // 3) Parse with yaml-cpp
    YAML::Node root;
    try {
        root = YAML::Load(content);
    } catch (const YAML::Exception& e) {
        std::cerr << "DEBUG YAML PARSE EXCEPTION: " << e.what() << "\n";
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
        std::cerr << "DEBUG YAML FIELD EXCEPTION: " << e.what() << "\n";
        return false;
    }

    std::cerr << "DEBUG: Parsed metadata: "
              << meta.name << "@" << meta.version
              << " arch=" << meta.arch
              << " deps=" << meta.deps.size()
              << " conflicts=" << meta.conflicts.size()
              << "\n";

    return true;
}

} // namespace anemo
