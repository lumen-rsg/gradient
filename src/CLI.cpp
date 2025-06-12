// src/CLI.cpp
// Created by cv2 on 6/12/25.

#include "CLI.h"
#include "Installer.h"
#include "Repository.h"
#include "Database.h"
#include "cxxopts.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/node/parse.h>

namespace fs = std::filesystem;

namespace anemo {

CLI::CLI(int argc, char* argv[])
    : argc_(argc)
    , argv_(argv)
    , force_(false)
    , parseOutput_(false)
{}

void CLI::run() {
    // Define global flags
    cxxopts::Options opts("anemo", "Anemo package manager");
    opts.positional_help("<command> [args]");
    opts.allow_unrecognised_options();
    opts.add_options()
        ("f,force",     "Force action (ignore warnings)",   cxxopts::value<bool>(force_))
        ("b,bootstrap", "Bootstrap directory prefix",       cxxopts::value<std::string>(bootstrapDir_))
        ("p,parse",     "Parseable output",                 cxxopts::value<bool>(parseOutput_))
        ("h,help",      "Print help");

    // Parse
    auto result = opts.parse(argc_, argv_);
    if (result.count("help")) {
        std::cout << opts.help() << "\n";
        return;
    }

    // Extract command + args
    auto unmatched = result.unmatched();
    if (unmatched.empty()) {
        std::cout << opts.help() << "\n";
        return;
    }
    std::string cmd = unmatched[0];
    std::vector<std::string> args(unmatched.begin() + 1, unmatched.end());

    // Prepare bootstrap paths
    std::string rootPrefix = bootstrapDir_.empty() ? "" : bootstrapDir_;

    fs::path dbDir = fs::path(rootPrefix) / "/var/lib/anemo/";
    std::error_code ec;
    if (!fs::exists(dbDir)) {
        fs::create_directories(dbDir, ec);
        if (ec) {
            std::cerr << "\033[31merror:\033[0m cannot create directory '"
                      << dbDir << "': " << ec.message() << "\n";
            return;
        }
    }
    fs::path dbPath = dbDir / "anemo.db";

    fs::path repoDir = fs::path(rootPrefix) / "var/lib/anemo/repos";
    if (!fs::exists(repoDir)) {
        fs::create_directories(repoDir, ec);
        if (ec) {
            std::cerr << "\033[31merror:\033[0m cannot create directory '"
                      << repoDir << "': " << ec.message() << "\n";
            return;
        }
    }

    // Open DB and Repo
    Database db(dbPath.string());
    if (!db.open() || !db.initSchema()) {
        std::cerr << "\033[31merror:\033[0m Unable to open or initialize database at "
                  << dbPath << "\n";
        return;
    }
    Repository repo(/* url */"", repoDir.string());

    // Dispatch commands
    if (cmd == "install") {
        if (args.empty()) {
            std::cerr << "\033[31merror:\033[0m 'install' requires at least one .apkg path\n";
            return;
        }
        std::string installRoot = rootPrefix.empty() ? "/" : rootPrefix;
        Installer inst(db, repo, force_, installRoot);
        for (auto& pkg : args) {
            if (!inst.installArchive(pkg)) {
                std::cerr << "\033[31merror:\033[0m Failed to install '" << pkg << "'\n";
            }
        }
    }

    else if (cmd == "remove") {
        if (!bootstrapDir_.empty()) {
            std::cerr << "\033[31merror:\033[0m Cannot remove packages when bootstrapping.\n";
            return;
        }
        if (args.empty()) {
            std::cerr << "\033[31merror:\033[0m 'remove' requires at least one package name\n";
            return;
        }
        Installer inst(db, repo, force_, /*rootDir=*/"/");
        for (auto& pkg : args) {
            if (!inst.removePackage(pkg)) {
                std::cerr << "\033[31merror:\033[0m Failed to remove '" << pkg << "'\n";
            }
        }
    }
    else if (cmd == "add-repo") {
        // Usage: anemo add-repo <name> <url> [priority]
        if (args.size() < 2) {
            std::cerr << "\033[31merror:\033[0m 'add-repo' requires a <name> and a <url>\n";
            return;
        }

        const std::string& name = args[0];
        const std::string& url  = args[1];
        int priority = 50;
        if (args.size() >= 3) {
            try {
                priority = std::stoi(args[2]);
            } catch (const std::exception&) {
                std::cerr << "\033[31merror:\033[0m invalid priority '"
                          << args[2] << "'\n";
                return;
            }
        }

        // Determine repos directory (bootstrapDir_ is a member of CLI)
        fs::path repoDir = bootstrapDir_.empty()
            ? fs::path("/var/lib/anemo/repos")
            : fs::path(bootstrapDir_) / "var/lib/anemo/repos";

        std::error_code ec;
        fs::create_directories(repoDir, ec);
        if (ec) {
            std::cerr << "\033[31merror:\033[0m unable to create directory '"
                      << repoDir << "': " << ec.message() << "\n";
            return;
        }

        // Check for existing repo file
        fs::path repoFile = repoDir / (name + ".json");
        if (fs::exists(repoFile)) {
            std::cerr << "\033[31merror:\033[0m repository '"
                      << name << "' already exists\n";
            return;
        }

        // Write out the JSON descriptor
        std::ofstream out(repoFile);
        if (!out) {
            std::cerr << "\033[31merror:\033[0m cannot open '"
                      << repoFile << "' for writing\n";
            return;
        }
        out << "{\n";
        out << "  \"name\":     \""   << name     << "\",\n";
        out << "  \"url\":      \""   << url      << "\",\n";
        out << "  \"priority\": "     << priority << "\n";
        out << "}\n";
        out.close();

        std::cout << "\033[32minfo:\033[0m repository '"
                  << name << "' added with priority "
                  << priority << "\n";
    }
    else if (cmd == "sync-repo") {
        // Determine the repos directory
    fs::path repoBase = bootstrapDir_.empty()
        ? fs::path("/var/lib/anemo/repos")
        : fs::path(bootstrapDir_) / "var/lib/anemo/repos";

    if (!fs::exists(repoBase) || !fs::is_directory(repoBase)) {
        std::cerr << "\033[31merror:\033[0m repos directory '"
                  << repoBase << "' does not exist\n";
        return;
    }

    std::cout << "\033[1;34m🔄 Syncing repositories from "
              << repoBase << "\033[0m\n";

    for (auto& entry : fs::directory_iterator(repoBase)) {
        auto repoFile = entry.path();
        if (repoFile.extension() != ".json")
            continue;  // skip non-JSON

        // Parse the local repo descriptor
        YAML::Node repoDesc;
        try {
            repoDesc = YAML::LoadFile(repoFile.string());
        } catch (const YAML::Exception& e) {
            std::cerr << "\033[31merror:\033[0m Failed to parse '"
                      << repoFile.filename() << "': " << e.what() << "\n";
            continue;
        }

        std::string name     = repoDesc["name"].as<std::string>();
        std::string url      = repoDesc["url"].as<std::string>();

        // Prepare local storage
        fs::path localDir    = repoBase / name;
        fs::path indexFile   = localDir / "repo.json";
        std::error_code ec;
        fs::create_directories(localDir, ec);
        if (ec) {
            std::cerr << "\033[31merror:\033[0m Cannot create directory '"
                      << localDir << "': " << ec.message() << "\n";
            continue;
        }

        // Fetch remote index
        std::string remoteIndexUrl = url + "/repo.json";
        std::cout << "  🔄 " << name
                  << ": fetching " << remoteIndexUrl << " ... " << std::flush;

        std::string cmd = "curl -fsSL '" + remoteIndexUrl +
                          "' -o '" + indexFile.string() + "'";
        int rc = std::system(cmd.c_str());
        if (rc != 0) {
            std::cout << "\033[31m✖ failed\033[0m\n";
        } else {
            std::cout << "\033[32m✔ done\033[0m\n";
        }
    }

    std::cout << "\033[1;34m🔄 Sync complete.\033[0m\n";
    }
    else if (cmd == "remove-repo") {
        // Usage: anemo remove-repo <name>
        if (args.size() < 1) {
            std::cerr << "\033[31merror:\033[0m 'remove-repo' requires a repository name\n";
            return;
        }
        const std::string& name = args[0];

        // Determine the repos directory
        fs::path repoBase = bootstrapDir_.empty()
            ? fs::path("/var/lib/anemo/repos")
            : fs::path(bootstrapDir_) / "var/lib/anemo/repos";

        // Ensure it exists
        if (!fs::exists(repoBase) || !fs::is_directory(repoBase)) {
            std::cerr << "\033[31merror:\033[0m repos directory '"
                      << repoBase << "' does not exist\n";
            return;
        }

        // Path to the descriptor
        fs::path repoJson = repoBase / (name + ".json");
        if (!fs::exists(repoJson)) {
            std::cerr << "\033[31merror:\033[0m repository '" << name
                      << "' not found in " << repoBase << "\n";
            return;
        }

        // Remove the JSON descriptor
        std::error_code ec;
        fs::remove(repoJson, ec);
        if (ec) {
            std::cerr << "\033[31merror:\033[0m failed to remove '"
                      << repoJson << "': " << ec.message() << "\n";
            return;
        }
        std::cout << "\033[32minfo:\033[0m removed repository descriptor '"
                  << repoJson.filename() << "'\n";

        // Remove any synced data directory (<repoBase>/<name>/)
        fs::path dataDir = repoBase / name;
        if (fs::exists(dataDir)) {
            fs::remove_all(dataDir, ec);
            if (ec) {
                std::cerr << "\033[33mwarning:\033[0m failed to remove data directory '"
                          << dataDir << "': " << ec.message() << "\n";
            } else {
                std::cout << "\033[32minfo:\033[0m removed repository data at '"
                          << dataDir << "'\n";
            }
        }

        std::cout << "\033[32msuccess:\033[0m repository '" << name << "' removed\n";
    }
    else if (cmd == "system-update") {
        std::cout << "\033[32minfo:\033[0m system-update command invoked\n";
    }
    else if (cmd == "audit") {
        // 1) Fetch broken packages
        auto broken = db.getBrokenPackages();
        if (broken.empty()) {
            std::cout << "\033[32minfo:\033[0m No broken packages found.\n";
            return;
        }

        // 2) Display them
        std::cout << "\033[31mbroken packages:\033[0m\n";
        for (auto& pkg : broken) {
            std::cout << "  - " << pkg << "\n";
        }

        // 3) Re-check dependencies & clear fixed ones
        std::vector<std::string> fixed;
        for (auto& pkg : broken) {
            auto deps = db.getDependencies(pkg);
            bool allOk = true;
            for (auto& dep : deps) {
                if (!db.isInstalled(dep, "")) {
                    allOk = false;
                    break;
                }
            }
            if (allOk) {
                if (db.removeBroken(pkg)) {
                    fixed.push_back(pkg);
                }
            }
        }

        // 4) Show results
        if (!fixed.empty()) {
            std::cout << "\033[32minfo:\033[0m Packages now fixed:\n";
            for (auto& pkg : fixed) {
                std::cout << "  + " << pkg << "\n";
            }
        }
    }

    else if (cmd == "info") {
         if (args.empty()) {
            std::cerr << "\033[31merror:\033[0m 'info' requires a package name\n";
            return;
            }

        // Index installed packages by name
        auto pkgs = db.listPackages();
        std::unordered_map<std::string, PackageInfo> idx;
        for (auto& p : pkgs) idx[p.name] = p;

        for (auto& name : args) {
            auto it = idx.find(name);
            if (it == idx.end()) {
                std::cerr << "\033[31merror:\033[0m Package '"
                << name << "' is not installed\n";
                continue;
                }
            auto& pkg = it->second;
            if (parseOutput_) {
                // name|version|arch
                std::cout
                << pkg.name    << '|'
                << pkg.version << '|'
                << pkg.arch    << "\n";
                } else {
                    std::cout << "\n"
                    << "\033[1;36m📄 Package:\033[0m \033[1m" << pkg.name << "\033[0m\n"
                    << "  \033[1mVersion:\033[0m " << pkg.version << "\n"
                    << "  \033[1mArch:\033[0m    " << pkg.arch << "\n";
                    }
            }
        }
    else if (cmd == "query") {
            // Usage: anemo query <pattern>
    if (args.empty()) {
        std::cerr << "\033[31merror:\033[0m 'query' requires a search pattern\n";
        return;
    }
    std::string pattern = args[0];
    std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);

    // Find repos directory
    fs::path repoBase = bootstrapDir_.empty()
        ? fs::path("/var/lib/anemo/repos")
        : fs::path(bootstrapDir_) / "var/lib/anemo/repos";

    if (!fs::exists(repoBase) || !fs::is_directory(repoBase)) {
        std::cerr << "\033[31merror:\033[0m repos directory '"
                  << repoBase << "' does not exist\n";
        return;
    }

    bool anyMatch = false;
    // Iterate over each repo descriptor (<name>.json)
    for (auto& entry : fs::directory_iterator(repoBase)) {
        if (entry.path().extension() != ".json")
            continue;

        std::string repoName = entry.path().stem();
        fs::path indexPath = repoBase / repoName / "repo.json";

        if (!fs::exists(indexPath)) {
            if (!parseOutput_) {
                std::cerr << "\033[33minfo:\033[0m repo '"
                          << repoName << "' not synced; skipping\n";
            }
            continue;
        }

        // Load the remote index (JSON is valid YAML)
        YAML::Node idx;
        try {
            idx = YAML::LoadFile(indexPath.string());
        } catch (const YAML::Exception& e) {
            std::cerr << "\033[31merror:\033[0m failed to parse '"
                      << indexPath.filename() << "': " << e.what() << "\n";
            continue;
        }

        auto packages = idx["packages"];
        if (!packages || !packages.IsSequence())
            continue;

        bool printedHeader = false;
        for (const auto& pkg : packages) {
            std::string name    = pkg["pkgname"].as<std::string>();
            std::string lowName = name;
            std::transform(lowName.begin(), lowName.end(), lowName.begin(), ::tolower);

            if (lowName.find(pattern) == std::string::npos)
                continue;

            anyMatch = true;
            std::string ver   = pkg["pkgver"].as<std::string>();
            std::string arch  = pkg["arch"].as<std::string>();
            std::string file  = pkg["filename"].as<std::string>();
            std::string desc  = pkg["description"].as<std::string>();

            if (parseOutput_) {
                // repo|name|version|arch|filename
                std::cout
                  << repoName << '|'
                  << name     << '|'
                  << ver      << '|'
                  << arch     << '|'
                  << file     << "\n";
            } else {
                if (!printedHeader) {
                    std::cout << "\033[1;35mRepository:\033[0m \033[1m"
                              << repoName << "\033[0m\n";
                    printedHeader = true;
                }
                std::cout
                  << "  \033[32m•\033[0m " << name
                  << " \033[90m" << ver << "\033[0m"
                  << " [" << arch << "]\n"
                  << "      " << desc << "\n";
            }
        }
    }

    if (!anyMatch && !parseOutput_) {
        std::cout << "\033[33minfo:\033[0m no packages matching '"
                  << args[0] << "' found in any repo\n";
    }

    }
    else {
        std::cerr << "\033[31merror:\033[0m Unknown command '" << cmd << "'\n";
    }
}

} // namespace anemo
