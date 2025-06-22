// src/Installer.cpp
// Created by cv2 on 6/12/25.

#include "Installer.h"
#include "TarHandler.h"
#include "ScriptExecutor.h"

#include <sys/utsname.h>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <ranges>
#include <unordered_set>
#include <utility>
#include <vector>
#include <regex>

#include "tools.h"

namespace fs = std::filesystem;

namespace gradient {

Installer::Installer(Database& db,
                     Repository& repo,
                     const bool force,
                     std::string rootDir,
                     const std::unordered_set<std::string>& staged)
    : db_(db)
    , repo_(repo)
    , resolver_(db, repo)
    , force_(force)
    , rootDir_(std::move(rootDir))
    , warnings_(false)
    , staged_ (staged)
{}

std::string Installer::detectHostArch() {
    utsname u{};
    uname(&u);
    return {u.machine};
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
    if (auto hostArch = detectHostArch(); (meta.arch != "any" && meta.arch != "all") && meta.arch != hostArch) {
        std::cerr << "\033[31merror:\033[0m Arch mismatch: package is '"
                  << meta.arch << "' but host is '" << hostArch << "'.\n";
        return false;
    }

    for (const auto& raw_dep : meta.deps) {
        Tools::Constraint c = Tools::parseConstraint(raw_dep);
        std::string& dep = c.name;

        // skip SONAMEs
        if (dep.find(".so") != std::string::npos) continue;

        // skip if package itself provides it
        if (std::ranges::find(meta.provides, dep)
            != meta.provides.end())
        {
            continue;
        }
        // skip if any installed pkg Provides it
        if (db_.isProvided(dep)) continue;

            if (db_.providesSatisfies(c)) {
                continue;   // dependency is satisfied by some provider at the right version
            }
        // skip if staged install
        if (staged_.contains(dep)) continue;

        // if installed, check version
        if (std::string instVer; db_.getPackageVersion(dep, instVer)) {
            if (Tools::evalConstraint(instVer, c)) {
                continue;  // satisfied
            } else {
                std::cerr << "\033[33mwarning:\033[0m dependency '"
                          << raw_dep << "' demands version " << c.op
                          << c.version << ", but found " << instVer << "\n";
                if (!force_) {
                    std::cerr << "\033[31merror:\033[0m Aborting due to version mismatch.\n";
                    return false;
                }
                warnings_ = true;
                continue;
            }
        }

        // not installed at all
        std::cerr << "\033[33mwarning:\033[0m Missing dependency '"
                  << raw_dep << "'\n";
        if (!force_) {
            std::cerr << "\033[31merror:\033[0m Aborting due to missing dependency.\n";
            return false;
        }
        warnings_ = true;
    }

    // === 4) Conflicts check with version support ===
    for (const auto& raw_conf : meta.conflicts) {
        Tools::Constraint c = Tools::parseConstraint(raw_conf);
        std::string& conf = c.name;
        if (std::string instVer; db_.getPackageVersion(conf, instVer)) {
            if (Tools::evalConstraint(instVer, c)) {
                std::cerr << "\033[33mwarning:\033[0m conflict with installed '"
                          << raw_conf << "'\n";
                if (!force_) {
                    std::cerr << "\033[31merror:\033[0m Aborting due to conflict.\n";
                    return false;
                }
                warnings_ = true;
            }
        }
    }

    // === 5) Replaces logic with version support ===
    for (const auto& raw_rep : meta.replaces) {
        Tools::Constraint c = Tools::parseConstraint(raw_rep);
        std::string& rep = c.name;
        if (std::string instVer; db_.getPackageVersion(rep, instVer) &&
                                 Tools::evalConstraint(instVer, c))
        {
            std::cout << "\033[32minfo:\033[0m Replacing '" << raw_rep << "'\n";
            removePackage(rep);
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
        if (!db_.rollbackTransaction()) {
            std::cerr << "\033[31merror:\033[0m Failed to rollback transaction.\n";
        }
        for (auto & installedFile : std::ranges::reverse_view(installedFiles)) {
            fs::remove(installedFile);
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

    // 12) Install files & log them by extracting via tar (preserves symlinks)
    {
        fs::path pkg_root = fs::path(tmp) / "package";
        bool hasFiles = fs::exists(pkg_root)
                     && fs::is_directory(pkg_root)
                     && !fs::is_empty(pkg_root);

        if (!hasFiles) {
            std::cerr << "\033[33minfo:\033[0m package contains no files; skipping file installation\n";
        } else {
            // (a) Stream pkgRoot into rootDir_, preserving ACLs, xattrs, symlinks, perms
            //
            //   tar --acls --xattrs -C pkgRoot -cf - . \
            //     | tar --acls --xattrs -C rootDir_ -xpf -
            //
            // -c: create, -f - : write to stdout
            // -x: extract, -f - : read from stdin, -p: preserve permissions
            // --acls, --xattrs: preserve ACLs and extended attributes
            std::string tarCmd =
                "tar --acls --xattrs -C '"  + pkg_root.string()  + "' -cf - . "
                "| "
                "tar --acls --xattrs -C '"  + rootDir_        + "' -xpf -";
            if (std::system(tarCmd.c_str()) != 0) {
                std::cerr << "\033[31merror:\033[0m Failed to extract package files via tar pipeline.\n";
                rollback();
                return false;
            }

            // (b) Walk pkgRoot to log every regular file and symlink we just installed
            for (auto& entry : fs::recursive_directory_iterator(pkg_root)) {
                if (!fs::is_regular_file(entry.path())
                 && !fs::is_symlink(entry.path()))
                {
                    continue;
                }
                auto rel = fs::relative(entry.path(), pkg_root);
                // recordPath is absolute on the target system
                std::string recordPath = (fs::path("/") / rel).string();
                if (!db_.logFile(meta.name, recordPath)) {
                    std::cerr << "\033[31merror:\033[0m Failed logging file '"
                              << recordPath << "'.\n";
                    rollback();
                    return false;
                }
                installedFiles.emplace_back(fs::path(rootDir_) / rel);
            }
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
        return db_.markBroken(meta.name);
    }

    // 15) Run post-install hook
    if (!storedScriptPath.empty()) {
        ScriptExecutor::runScript(storedScriptPath, "post_install", rootDir_);
    }

    // 16) Success
    std::cout << "\033[32msuccess:\033[0m Installed '"
              << meta.name << "-" << meta.version << "'.\n";

    return true;
}

bool Installer::removePackage(const std::string& name) const {
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
