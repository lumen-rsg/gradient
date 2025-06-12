// src/Installer.cpp
// Created by cv2 on 6/12/25.

#include "Installer.h"
#include "TarHandler.h"
#include "ScriptExecutor.h"
#include <sys/utsname.h>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

namespace anemo {

    bool Installer::removePackage(const std::string& name) {
        return true;
    }

Installer::Installer(Database& db, Repository& repo, bool force, const std::string& rootDir)
    : db_(db), repo_(repo), resolver_(db, repo), force_(force), rootDir_(rootDir), warnings_(false) {}

std::string Installer::detectHostArch() {
    struct utsname u;
    uname(&u);
    return std::string(u.machine);
}

std::string Installer::makeTempDir() {
    char tmpl[] = "/tmp/anemoXXXXXX";
    mkdtemp(tmpl);
    return std::string(tmpl);
}

bool Installer::installArchive(const std::string& archivePath) {
    // Reset warnings
    warnings_ = false;

    Package pkg(archivePath);
    if (!pkg.loadMetadata()) {
        std::cerr << "\033[31merror:\033[0m Failed to read package metadata." << std::endl;
        return false;
    }
    auto meta = pkg.metadata();

    // 1. Architecture check
    auto hostArch = detectHostArch();
    if (meta.arch != "any" && meta.arch != hostArch) {
        std::cerr << "\033[31merror:\033[0m Arch mismatch: package is '" \
                  << meta.arch << "' but host is '" << hostArch << "'." << std::endl;
        return false;
    }

    // 2. Dependencies check
    for (const auto& dep : meta.deps) {
        if (!db_.isInstalled(dep, "")) {
            std::cerr << "\033[33mwarning:\033[0m Missing dependency '" << dep << "'." << std::endl;
            if (!force_) {
                std::cerr << "\033[31merror:\033[0m Aborting due to missing dependency." << std::endl;
                return false;
            }
            warnings_ = true;
        }
    }

    // 3. Conflicts check
    for (const auto& c : meta.conflicts) {
        if (db_.isInstalled(c, "")) {
            std::cerr << "\033[33mwarning:\033[0m Conflict with installed package '" << c << "'." << std::endl;
            if (!force_) {
                std::cerr << "\033[31merror:\033[0m Aborting due to conflict." << std::endl;
                return false;
            }
            warnings_ = true;
        }
    }

    // 4. Replaces logic
    for (const auto& r : meta.replaces) {
        if (db_.isInstalled(r, "")) {
            std::cout << "\033[32minfo:\033[0m Replacing package '" << r << "'." << std::endl;
            removePackage(r);
        }
    }

    // 5. Extract package contents
    auto tmp = makeTempDir();
    if (!TarHandler::extract(archivePath, tmp)) {
        std::cerr << "\033[31merror:\033[0m Failed to extract package." << std::endl;
        return false;
    }

    // Tracking installed file paths for rollback
    std::vector<fs::path> installedFiles;
    auto rollback = [&]() {
        db_.rollbackTransaction();
        for (auto it = installedFiles.rbegin(); it != installedFiles.rend(); ++it) {
            fs::remove(*it);
        }
    };

    // 6. Begin DB transaction
    if (!db_.beginTransaction()) {
        std::cerr << "\033[31merror:\033[0m Failed to begin DB transaction." << std::endl;
        return false;
    }

    // 7. Install files
    fs::path pkgRoot = fs::path(tmp) / "package";
    for (auto& entry : fs::recursive_directory_iterator(pkgRoot)) {
        if (fs::is_regular_file(entry.path())) {
            auto rel = fs::relative(entry.path(), pkgRoot);
            auto dest = fs::path(rootDir_) / rel;
            fs::create_directories(dest.parent_path());
            std::string cmd = "/bin/install -D " + entry.path().string() + " " + dest.string();
            if (std::system(cmd.c_str()) != 0) {
                std::cerr << "\033[31merror:\033[0m Failed to install '" << entry.path() << "'." << std::endl;
                rollback();
                return false;
            }
            if (!db_.logFile(meta.name, dest.string())) {
                std::cerr << "\033[31merror:\033[0m Failed logging file '" << dest << "'." << std::endl;
                rollback();
                return false;
            }
            installedFiles.push_back(dest);
        }
    }

    // 8. Record package metadata
    if (!db_.addPackage(meta)) {
        std::cerr << "\033[31merror:\033[0m Failed to add package record." << std::endl;
        rollback();
        return false;
    }

    // 9. Commit transaction
    if (!db_.commitTransaction()) {
        std::cerr << "\033[31merror:\033[0m Failed to commit DB transaction." << std::endl;
        rollback();
        return false;
    }

    // 10. Mark broken if forced with warnings
    if (warnings_ && force_) {
        std::cout << "\033[33mwarning:\033[0m Package installed with warnings; marking as broken." << std::endl;
        db_.markBroken(meta.name);
    }

    // 11. Run post-install script
    ScriptExecutor::runScript(tmp + "/install.anemonix", "post_install");

    std::cout << "\033[32msuccess:\033[0m Installed '" << meta.name << "-" << meta.version << "'." << std::endl;
    return true;
}

} // namespace anemo
