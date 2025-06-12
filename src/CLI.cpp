// src/CLI.cpp
// Created by cv2 on 6/12/25.

#include "CLI.h"
#include "Installer.h"
#include "Repository.h"
#include "Database.h"
#include "cxxopts.h"
#include <iostream>

namespace anemo {

CLI::CLI(int argc, char* argv[])
    : argc_(argc), argv_(argv), force_(false), parseOutput_(false) {}

void CLI::run() {
    // 1) Define global flags
    cxxopts::Options opts("anemo", "Anemo package manager");
    opts.positional_help("<command> [args]");
    opts.allow_unrecognised_options();
    opts.add_options()
        ("f,force", "Force action (ignore warnings)", cxxopts::value<bool>(force_))
        ("b,bootstrap", "Bootstrap directory prefix", cxxopts::value<std::string>(bootstrapDir_))
        ("p,parse", "Parseable output", cxxopts::value<bool>(parseOutput_))
        ("h,help", "Print help");

    // 2) Parse
    auto result = opts.parse(argc_, argv_);
    if (result.count("help")) {
        std::cout << opts.help() << "\n";
        return;
    }

    // 3) Extract command + its args
    auto unmatched = result.unmatched();
    if (unmatched.empty()) {
        std::cout << opts.help() << "\n";
        return;
    }
    std::string cmd = unmatched[0];
    std::vector<std::string> args(unmatched.begin() + 1, unmatched.end());

    // 4) Initialize core services
    const std::string dbPath = bootstrapDir_.empty()
        ? "/var/lib/anemo/anemo.db"
        : (bootstrapDir_ + "/anemo.db");
    Database db(dbPath);
    if (!db.open() || !db.initSchema()) {
        std::cerr << "\033[31merror:\033[0m Unable to open or initialize database at "
                  << dbPath << "\n";
        return;
    }

    const std::string repoDir = bootstrapDir_.empty()
        ? "/var/lib/anemo/repos"
        : (bootstrapDir_ + "/repos");
    Repository repo(/* url: */"", repoDir);

    // 5) Dispatch
    if (cmd == "install") {
        if (args.empty()) {
            std::cerr << "\033[31merror:\033[0m 'install' requires at least one .apkg path\n";
            return;
        }
        Installer inst(db, repo, force_, bootstrapDir_.empty() ? "/" : bootstrapDir_);
        for (auto& pkg : args) {
            if (!inst.installArchive(pkg)) {
                std::cerr << "\033[31merror:\033[0m Failed to install '" << pkg << "'\n";
                // continue to next or exit early?
                // break;
            }
        }
    }
    else if (cmd == "remove") {
        // TODO: implement removal logic
        std::cout << "\033[32minfo:\033[0m remove command invoked\n";
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
