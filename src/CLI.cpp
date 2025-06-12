// src/CLI.cpp
// Created by cv2 on 6/12/25.

#include "CLI.h"
#include "Installer.h"
#include "Repository.h"
#include "Database.h"
#include "cxxopts.h"

#include <iostream>
#include <filesystem>

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
        std::cout << "\033[32minfo:\033[0m add-repo command invoked\n";
    }
    else if (cmd == "sync-repo") {
        std::cout << "\033[32minfo:\033[0m sync-repo command invoked\n";
    }
    else if (cmd == "remove-repo") {
        std::cout << "\033[32minfo:\033[0m remove-repo command invoked\n";
    }
    else if (cmd == "system-update") {
        std::cout << "\033[32minfo:\033[0m system-update command invoked\n";
    }
    else if (cmd == "audit") {
        std::cout << "\033[32minfo:\033[0m audit command invoked\n";
    }
    else if (cmd == "list") {
        std::cout << "\033[32minfo:\033[0m list command invoked\n";
    }
    else if (cmd == "info") {
        std::cout << "\033[32minfo:\033[0m info command invoked\n";
    }
    else if (cmd == "query") {
        std::cout << "\033[32minfo:\033[0m query command invoked\n";
    }
    else {
        std::cerr << "\033[31merror:\033[0m Unknown command '" << cmd << "'\n";
    }
}

} // namespace anemo
