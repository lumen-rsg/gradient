// src/Package.cpp
// Created by cv2 on 6/12/25.

#include "Package.h"
#include "YamlParser.h"
#include "TarHandler.h"

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace gradient {

    Package::Package(const std::string& archivePath)
      : archivePath_(archivePath) {}

    bool Package::loadMetadata() {
        // 1) Create a temporary directory for extraction
        char tmpl[] = "/tmp/gradient_metaXXXXXX";
        char* tmpDirC = mkdtemp(tmpl);
        if (!tmpDirC) {
            std::cerr << "\033[31merror:\033[0m could not create temp dir for metadata\n";
            return false;
        }
        std::string tmpDir(tmpDirC);

        // 2) Extract the entire .apkg into tmpDir
        if (!TarHandler::extract(archivePath_, tmpDir)) {
            std::cerr << "\033[31merror:\033[0m failed to extract '"
                      << archivePath_ << "' for metadata\n";
            return false;
        }

        // 3) Locate anemonix.yaml anywhere in the extracted tree
        fs::path metaPath;
        for (auto& entry : fs::recursive_directory_iterator(tmpDir)) {
            if (entry.is_regular_file()
                && entry.path().filename() == "anemonix.yaml")
            {
                metaPath = entry.path();
                break;
            }
        }
        if (metaPath.empty()) {
            std::cerr << "\033[31merror:\033[0m anemonix.yaml not found in '"
                      << archivePath_ << "'\n";
            return false;
        }

        // 4) Parse the metadata
        if (!YamlParser::parseMetadata(metaPath.string(), meta_)) {
            std::cerr << "\033[31merror:\033[0m failed to parse metadata at '"
                      << metaPath << "'\n";
            return false;
        }

        return true;
    }

    const Package::Metadata& Package::metadata() const {
        return meta_;
    }

} // namespace anemo
