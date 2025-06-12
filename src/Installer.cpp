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

Installer::Installer(Database& db,
                     Repository& repo,
                     bool force,
                     const std::string& rootDir)
    : db_(db)
    , repo_(repo)
    , resolver_(db, repo)
    , force_(force)
    , rootDir_(rootDir)
    , warnings_(false)
{}

std::string Installer::detectHostArch() {
    struct utsname u;
    uname(&u);
    return std::string(u.machine);
}

std::string Installer::makeTempDir() {
    char tmpl[] = "/tmp/anemoXXXXXX";
    char* dir = mkdtemp(tmpl);
    return dir ? std::string(dir) : std::string{};
}

bool Installer::installArchive(const std::string& archivePath) {
    warnings_ = false;

    // 1) Load metadata
    Package pkg(archivePath);
    if (!pkg.loadMetadata()) {
        std::cerr << "\033[31merror:\033[0m Failed to read package metadata.\n";
        return false;
    }
    auto meta = pkg.metadata();

    // 2) Architecture check
    auto hostArch = detectHostArch();
    if ((meta.arch != "any" && meta.arch != "all") && meta.arch != hostArch) {
        std::cerr << "\033[31merror:\033[0m Arch mismatch: package is '"
                  << meta.arch << "' but host is '" << hostArch << "'.\n";
        return false;
    }

    // 3) Dependencies check
    for (const auto& dep : meta.deps) {
        if (!db_.isInstalled(dep, "")) {
            std::cerr << "\033[33mwarning:\033[0m Missing dependency '"
                      << dep << "'.\n";
            if (!force_) {
                std::cerr << "\033[31merror:\033[0m Aborting due to missing dependency.\n";
                return false;
            }
            warnings_ = true;
        }
    }

    // 4) Conflicts check
    for (const auto& c : meta.conflicts) {
        if (db_.isInstalled(c, "")) {
            std::cerr << "\033[33mwarning:\033[0m Conflict with installed package '"
                      << c << "'.\n";
            if (!force_) {
                std::cerr << "\033[31merror:\033[0m Aborting due to conflict.\n";
                return false;
            }
            warnings_ = true;
        }
    }

    // 5) Replaces logic
    for (const auto& r : meta.replaces) {
        if (db_.isInstalled(r, "")) {
            std::cout << "\033[32minfo:\033[0m Replacing package '"
                      << r << "'.\n";
            removePackage(r);
        }
    }

    // 6) Extract entire archive
    auto tmp = makeTempDir();
    if (tmp.empty() || !TarHandler::extract(archivePath, tmp)) {
        std::cerr << "\033[31merror:\033[0m Failed to extract package.\n";
        return false;
    }

    // 7) Locate any install.anemonix script under tmp
    fs::path scriptSrc;
    for (auto& entry : fs::recursive_directory_iterator(tmp)) {
        if (entry.is_regular_file() &&
            entry.path().filename() == "install.anemonix")
        {
            scriptSrc = entry.path();
            break;
        }
    }

    // 7) Persist install script (if present)
    std::string storedScriptPath;
    if (fs::exists(scriptSrc) && fs::is_regular_file(scriptSrc)) {
        fs::path scriptsDir = fs::path(rootDir_) / "var/lib/anemo/scripts";
        fs::create_directories(scriptsDir);
        std::string scriptName = meta.name + "-" + meta.version + ".anemonix";
        fs::path scriptDst = scriptsDir / scriptName;
        fs::copy_file(scriptSrc, scriptDst, fs::copy_options::overwrite_existing);
        storedScriptPath = scriptDst.string();
    }

    // 8) Prepare for rollback
    std::vector<fs::path> installedFiles;
    auto rollback = [&]() {
        db_.rollbackTransaction();
        for (auto it = installedFiles.rbegin(); it != installedFiles.rend(); ++it) {
            fs::remove(*it);
        }
        if (!storedScriptPath.empty()) {
            fs::remove(storedScriptPath);
        }
    };

    // 9) Begin transaction
    if (!db_.beginTransaction()) {
        std::cerr << "\033[31merror:\033[0m Failed to begin DB transaction.\n";
        return false;
    }

    // 10) Record package metadata & dependencies before logging files
    if (!db_.addPackage(meta, storedScriptPath)) {
        std::cerr << "\033[31merror:\033[0m Failed to add package record.\n";
        rollback();
        return false;
    }

    // 11) Locate package/ directory
    fs::path pkgRoot;
    for (auto& entry : fs::recursive_directory_iterator(tmp)) {
        if (entry.is_directory() && entry.path().filename() == "package") {
            pkgRoot = entry.path();
            break;
        }
    }
    if (pkgRoot.empty()) {
        fs::path candidate = fs::path(tmp) / "package";
        pkgRoot = (fs::exists(candidate) && fs::is_directory(candidate))
                  ? candidate
                  : fs::path(tmp);
    }

    // 12) Install files & log them (handling symlinks)
    bool hasFiles = fs::exists(pkgRoot)
                 && fs::is_directory(pkgRoot)
                 && !fs::is_empty(pkgRoot);
    if (!hasFiles) {
        std::cerr << "\033[33minfo:\033[0m package contains no files; skipping file installation\n";
    } else {
        for (auto& entry : fs::recursive_directory_iterator(pkgRoot)) {
            fs::path src = entry.path();
            fs::path rel = fs::relative(src, pkgRoot);
            fs::path dest = fs::path(rootDir_) / rel;
            fs::create_directories(dest.parent_path());

            if (fs::is_symlink(src)) {
                // Recreate the symlink at dest
                auto target = fs::read_symlink(src);
                // If dest exists (maybe from a previous run), remove it first
                if (fs::exists(dest)) fs::remove(dest);
                fs::create_symlink(target, dest);
            }
            else if (fs::is_regular_file(src)) {
                // Copy regular file
                std::string cmd = "/bin/install -D " + src.string() + " " + dest.string();
                if (std::system(cmd.c_str()) != 0) {
                    std::cerr << "\033[31merror:\033[0m Failed to install '"
                              << src << "'.\n";
                    rollback();
                    return false;
                }
            }
            else {
                // Skip directories (we already created them)
                continue;
            }

            // Log the path as seen by the system (always absolute from /)
            std::string recordPath = (fs::path("/") / rel).string();
            if (!db_.logFile(meta.name, recordPath)) {
                std::cerr << "\033[31merror:\033[0m Failed logging file '"
                          << recordPath << "'.\n";
                rollback();
                return false;
            }

            // Track for rollback (we always removed or created at dest)
            installedFiles.push_back(dest);
        }
    }

    // 13) Commit transaction
    if (!db_.commitTransaction()) {
        std::cerr << "\033[31merror:\033[0m Failed to commit DB transaction.\n";
        rollback();
        return false;
    }

    // 14) Mark broken if forced with warnings
    if (warnings_ && force_) {
        std::cout << "\033[33mwarning:\033[0m Package installed with warnings; marking as broken.\n";
        db_.markBroken(meta.name);
    }

    // 15) Run post-install hook
    if (!storedScriptPath.empty()) {
        ScriptExecutor::runScript(storedScriptPath, "post_install", rootDir_);
    } else {
        std::cerr << "\033[33minfo:\033[0m no install.anemonix script found; skipping post-install hook\n";
    }

    // 16) Success
    std::cout << "\033[32msuccess:\033[0m Installed '"
              << meta.name << "-" << meta.version << "'.\n";

    return true;
}

bool Installer::removePackage(const std::string& name) {
    // 1) Check installed
    if (!db_.isInstalled(name, "")) {
        std::cerr << "\033[31merror:\033[0m Package '" << name << "' is not installed.\n";
        return false;
    }

    // 2) Reverse-dependency check
    auto rev = db_.getReverseDependencies(name);
    if (!rev.empty()) {
        if (!force_) {
            std::cerr << "\033[31merror:\033[0m Cannot remove '" << name
                      << "'; other packages depend on it:\n";
            for (auto& pkg : rev) {
                std::cerr << "  - " << pkg << "\n";
            }
            return false;
        } else {
            std::cerr << "\033[33mwarning:\033[0m Force removing '" << name
                      << "'; marking dependents as broken.\n";
            for (auto& pkg : rev) {
                db_.markBroken(pkg);
            }
        }
    }

    // 3) Run pre-remove hook
    auto script = db_.getInstallScript(name);

    // 4) Begin DB transaction
    if (!db_.beginTransaction()) {
        std::cerr << "\033[31merror:\033[0m Failed to begin DB transaction.\n";
        return false;
    }

    // 5) Remove files from disk & DB
    auto files = db_.getFiles(name);
    for (auto& f : files) {
        // f is stored as absolute ("/path/to/file")
        fs::path dest = fs::path(rootDir_) / f.substr(1);
        if (fs::exists(dest) && !fs::remove(dest)) {
            std::cerr << "\033[33mwarning:\033[0m Failed to remove file '"
                      << dest << "'.\n";
        }
    }
    if (!db_.removeFiles(name)) {
        std::cerr << "\033[31merror:\033[0m Failed to remove file records.\n";
        db_.rollbackTransaction();
        return false;
    }

    // if (!db_.removeReverseDependencies(name)) {
    //     std::cerr << "\033[31merror:\033[0m Failed to remove reverse‐dependency records for '"
    //               << name << "'.\n";
    //     db_.rollbackTransaction();
    //     return false;
    // }

    // 9) Run post-remove hook
    if (!script.empty() && fs::exists(script)) {
        std::cout << script << std::endl;
        ScriptExecutor::runScript(script, "post_remove");
    }

    // 6) Remove stored script file
    if (!script.empty()) {
        if (!fs::remove(script)) {
            std::cerr << "\033[33mwarning:\033[0m Failed to remove script '"
                      << script << "'.\n";
        }
    }

    // 7) Delete from packages table
    if (!db_.deletePackage(name)) {
        std::cerr << "\033[31merror:\033[0m Failed to remove package record.\n";
        db_.rollbackTransaction();
        return false;
    }

    // 8) Commit DB transaction
    if (!db_.commitTransaction()) {
        std::cerr << "\033[31merror:\033[0m Failed to commit DB transaction.\n";
        db_.rollbackTransaction();
        return false;
    }

    std::cout << "\033[32msuccess:\033[0m Removed '" << name << "'.\n";
    return true;
}

} // namespace anemo
