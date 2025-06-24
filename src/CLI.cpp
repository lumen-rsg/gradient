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
#include <future>
#include <yaml-cpp/yaml.h>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/node/parse.h>

#include "tools.h"
#include "DownloadHelper.h"

namespace fs = std::filesystem;

namespace gradient {

CLI::CLI(int argc, char* argv[])
    : argc_(argc)
    , argv_(argv)
    , force_(false)
    , parseOutput_(false)
{}

void checkUID() {
    if (geteuid() != 0) {
        std::cerr << "\033[31merror:\033[0m this operation requires root privileges\n";\
        exit(EXIT_FAILURE);
    }
}



void CLI::run() {
    // Define global flags
    cxxopts::Options opts("gradient", "gradient package manager - epoch III. (version 2.0)");
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
    // if (!rootPrefix.empty()) {
    //     rootPrefix.pop_back();
    // }
    fs::path dbDir = fs::path(rootPrefix).string() + "/var/lib/gradient/";
    std::error_code ec;
    if (!fs::exists(dbDir)) {
        fs::create_directories(dbDir, ec);
        if (ec) {
            std::cerr << "\033[31merror:\033[0m cannot create directory '"
                      << dbDir << "': " << ec.message() << "\n";
            return;
        }
    }
    fs::path dbPath = dbDir / "gradient.db";

    fs::path repoDir = fs::path(rootPrefix).string() + "/var/lib/gradient/repos";
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
    if (cmd == "install-bin") {
        checkUID();
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
    else if (cmd == "install") {
        checkUID();
        static std::mutex cout_mtx;
        // 1) Locate repos base directory
        fs::path repoBase = fs::path("/var/lib/gradient/repos");
        if (!fs::exists(repoBase) || !fs::is_directory(repoBase)) {
            std::cerr << "\033[31merror:\033[0m system repos directory '"
                      << repoBase << "' does not exist\n";
            return;
        }

        if (!fs::exists(repoBase) || !fs::is_directory(repoBase)) {
            std::cerr << "\033[31merror:\033[0m repos directory '"
                      << repoBase << "' does not exist\n";
            return;
        }

        // 2) Build a mapping of package name -> list of available repo entries
        struct RepoPkg {
            std::string pkgname, pkgver, arch, filename, repoUrl;
            std::vector<std::string> depends;
            std::vector<std::string> provides;
            int priority{};
            std::string repoName;
        };
        std::unordered_map<std::string, std::vector<RepoPkg>> pkgMap;

        for (auto& entry : fs::directory_iterator(repoBase)) {
            if (entry.path().extension() != ".json") continue;
            std::string repoName = entry.path().stem();
            // Load the repo descriptor (name/url/priority)
            YAML::Node desc;
            try { desc = YAML::LoadFile(entry.path().string()); }
            catch (const YAML::Exception& e) {
                std::cerr << "\033[31merror:\033[0m parsing " << entry.path().filename()
                          << ": " << e.what() << "\n";
                continue;
            }
            auto url             = desc["url"].as<std::string>();
            int priority         = desc["priority"].as<int>();

            // Load the remote index we synced earlier
            fs::path indexFile = repoBase / repoName / "repo.json";
            if (!fs::exists(indexFile)) continue;

            YAML::Node idx;
            try { idx = YAML::LoadFile(indexFile.string()); }
            catch (const YAML::Exception& e) {
                std::cerr << "\033[31merror:\033[0m parsing " << indexFile.filename()
                          << ": " << e.what() << "\n";
                continue;
            }
            auto packages = idx["packages"];
            if (!packages || !packages.IsSequence()) continue;

            for (const auto& node : packages) {
                RepoPkg rp;
                rp.pkgname   = node["pkgname"].as<std::string>();
                rp.pkgver    = node["pkgver"].as<std::string>();
                rp.arch      = node["arch"].as<std::string>();
                rp.filename  = node["filename"].as<std::string>();
                rp.repoUrl   = url;
                rp.priority  = priority;
                rp.repoName  = repoName;

                // Dependencies
                if (node["depends"]) {
                    for (const auto& dnode : node["depends"])
                        rp.depends.push_back(dnode.as<std::string>());
                }

                // Provides (strip version suffix after '=')
                if (node["provides"]) {
                    for (const auto& pnode : node["provides"]) {
                        auto prov = pnode.as<std::string>();
                        if (auto eq = prov.find('='); eq != std::string::npos)
                            prov.resize(eq);
                        rp.provides.push_back(prov);
                    }
                }

                // 1) Always index under its real name
                pkgMap[rp.pkgname].push_back(rp);

                // 2) Also index under each provided name, but skip the case prov==pkgname
                for (const auto& prov : rp.provides) {
                    if (prov == rp.pkgname)
                        continue;    // <-- this line avoids self‐cycle on "libcap"
                    pkgMap[prov].push_back(rp);
                }
            }
        }

        // 3) Resolve dependencies (DFS, picking highest-priority entries)
        std::vector<RepoPkg> installOrder;
    std::unordered_set<std::string> visited, inStack;

    auto dfs = [&](auto& self, const std::string& raw_req) -> bool {
    // 1) Parse name + optional version operator
    const Tools::Constraint c = Tools::parseConstraint(raw_req);
    const std::string& name = c.name;

    // 2) If we've already visited or the package is installed and satisfies the version, skip it
    if (visited.contains(name))
        return true;

    if (std::string instVer; db.getPackageVersion(name, instVer) &&
                             (c.op.empty() || Tools::evalConstraint(instVer, c)))
    {
        visited.insert(name);
        return true;
    }

    // 3) Lookup candidates by base name
    auto it = pkgMap.find(name);
    if (it == pkgMap.end()) {
        std::cerr << "\033[31merror:\033[0m package '"
                  << raw_req << "' not found in any repo\n";
        return false;
    }

    // 4) Filter by version constraint
    std::vector<RepoPkg> candidates;
    for (auto& rp : it->second) {
        if (c.op.empty() || Tools::evalConstraint(rp.pkgver, c)) {
            candidates.push_back(rp);
        }
    }
    if (candidates.empty()) {
        std::cerr << "\033[31merror:\033[0m no candidate for '"
                  << raw_req << "'\n";
        return false;
    }

    // 5) Prefer real pkgname == name over pure providers
    std::vector<RepoPkg> realOnly;
    for (auto& rp : candidates) {
        if (rp.pkgname == name) realOnly.push_back(rp);
    }
    if (!realOnly.empty()) candidates = std::move(realOnly);

    // 6) Sort by priority, then version
    std::sort(candidates.begin(), candidates.end(),
        [](auto const& a, auto const& b) {
            if (a.priority != b.priority)
                return a.priority > b.priority;
            return Tools::versionCompare(a.pkgver, b.pkgver) > 0;
        });
    const RepoPkg& best = candidates[0];

    // 7) Cycle detection
    if (inStack.count(name)) {
        std::lock_guard<std::mutex> lk(cout_mtx);
        std::cout << "  \033[33mwarning:\033[0m cycle on '" << name
                  << "', skipping\n";
        visited.insert(name);
        return true;
    }
    inStack.insert(name);

    // 8) Recurse into its dependencies (passing raw strings so version matters)
    for (auto const& raw_dep : best.depends) {
        Tools::Constraint dc = Tools::parseConstraint(raw_dep);
        const std::string& dn = dc.name;

        // skip SONAMEs
        if (dn.find(".so") != std::string::npos) continue;
        // skip self‐depend
        if (dn == name) continue;
        // installed & matching version?
        std::string dv;
        if (db.getPackageVersion(dn, dv) &&
            (dc.op.empty() || Tools::evalConstraint(dv, dc)))
        {
            continue;
        }

        if (!self(self, raw_dep))
            return false;
    }

    // 9) Done with this package
    inStack.erase(name);
    visited.insert(name);
    installOrder.push_back(best);
    return true;
};

// Kick it off with raw args (which may include version qualifiers)
for (auto const& r : args) {
    if (!dfs(dfs, r))
        return;
}

        std::vector<RepoPkg> toInstall;
        toInstall.reserve(installOrder.size());

        for (auto const& p : installOrder) {
            std::string instVer;
            bool already = db.getPackageVersion(p.pkgname, instVer)
                           && instVer == p.pkgver;
            if (already) {
                // Inform the user (optional)
                std::cout << "\033[32minfo:\033[0m "
                          << p.pkgname << "-" << p.pkgver
                          << " already installed; skipping\n";
            } else {
                toInstall.push_back(p);
            }
        }

        // Swap back into installOrder
        installOrder.swap(toInstall);

        if (installOrder.empty()) {
            std::cout << "\033[32minfo:\033[0m all requested packages are already installed\n";
            return;
        }


        fs::path tmp = fs::temp_directory_path() / "grad_pkgs";
        if (!fs::exists(tmp)) {
            fs::create_directory(tmp);
        }

        // Initialize curl once
        curl_global_init(CURL_GLOBAL_DEFAULT);

        // We'll collect futures here
        std::vector<std::future<bool>> futures;
        futures.reserve(installOrder.size());

        // A mutex to serialize progress‐bar prints
        static std::mutex printMutex;

        // Launch one download task per package
        for (size_t i = 0; i < installOrder.size(); ++i) {
            const auto& p = installOrder[i];
            std::string url = p.repoUrl + "/" + p.filename;
            fs::path out   = tmp / p.filename;

            // Copy the context by value so each thread has its own
            DownloadContext ctx{
                int(i+1),
                int(installOrder.size()),
                p.pkgname + "-" + p.pkgver,
                &printMutex
            };

            // async launch
            futures.push_back(std::async(std::launch::async,
                [url, out, ctx]() mutable {
                    return downloadWithCurl(url, out.string(), ctx);
                }
            ));
        }

        // Wait for all to finish
        bool allOk = true;
        for (auto& fut : futures) {
            if (!fut.get()) {
                allOk = false;
                break;
            }
        }

        curl_global_cleanup();

        if (!allOk) {
            std::cerr << "\n\033[31merror:\033[0m one or more downloads failed; aborting install\n";
            return;
        }

        std::unordered_set<std::string> staged;
        for (auto const& p : installOrder)
            staged.insert(p.pkgname);

        // 5) Install each downloaded archive in order
        std::string installRoot = bootstrapDir_.empty() ? "/" : bootstrapDir_;
        Installer inst(db, repo, force_, installRoot, staged);

        for (auto const& p : installOrder) {
            fs::path pkgPath = tmp / p.filename;
            std::cout << "\n\033[1;34m📦 Installing \033[1m"
                      << p.pkgname << "-" << p.pkgver << "\033[0m\n";
            if (!inst.installArchive(pkgPath.string())) {
                std::cerr << "\033[31merror:\033[0m Failed to install '"
                          << p.pkgname << "'\n";
                return;
            }
        }

        std::cout << "\033[32msuccess:\033[0m All packages installed.\n";

    }
    else if (cmd == "remove") {
        checkUID();
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
        checkUID();
        // Usage: gradient add-repo <name> <url> [priority]
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
        fs::path repo_dir = bootstrapDir_.empty()
            ? fs::path("/var/lib/gradient/repos")
            : fs::path(bootstrapDir_) / "var/lib/gradient/repos";

        std::error_code error_code;
        fs::create_directories(repo_dir, error_code);
        if (error_code) {
            std::cerr << "\033[31merror:\033[0m unable to create directory '"
                      << repo_dir << "': " << error_code.message() << "\n";
            return;
        }

        // Check for existing repo file
        fs::path repoFile = repo_dir / (name + ".json");
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
        out << R"(  "name":     ")"   << name     << "\",\n";
        out << R"(  "url":      ")"   << url      << "\",\n";
        out << "  \"priority\": "     << priority << "\n";
        out << "}\n";
        out.close();

        std::cout << "\033[32minfo:\033[0m repository '"
                  << name << "' added with priority "
                  << priority << "\n";
    }
    else if (cmd == "sync-repo") {
        checkUID();
        // Determine the repos directory
    fs::path repoBase = bootstrapDir_.empty()
        ? fs::path("/var/lib/gradient/repos")
        : fs::path(bootstrapDir_) / "var/lib/gradient/repos";

    if (!fs::exists(repoBase) || !fs::is_directory(repoBase)) {
        std::cerr << "\033[31merror:\033[0m repos directory '"
                  << repoBase << "' does not exist\n";
        return;
    }

    std::cout << "\033[1;34m🔄 Syncing repositories from "
              << repoBase << "\033[0m\n";

    for (auto& entry : fs::directory_iterator(repoBase)) {
        const auto& repoFile = entry.path();
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

        auto name     = repoDesc["name"].as<std::string>();
        auto url      = repoDesc["url"].as<std::string>();

        // Prepare local storage
        fs::path localDir    = repoBase / name;
        fs::path indexFile   = localDir / "repo.json";
        std::error_code error_code;
        fs::create_directories(localDir, error_code);
        if (error_code) {
            std::cerr << "\033[31merror:\033[0m Cannot create directory '"
                      << localDir << "': " << error_code.message() << "\n";
            continue;
        }

        // Fetch remote index
        std::string remoteIndexUrl = url + "/repo.json";
        std::cout << "  🔄 " << name
                  << ": fetching " << remoteIndexUrl << " ... " << std::flush;

        std::string command = "curl -fsSL '" + remoteIndexUrl +
                          "' -o '" + indexFile.string() + "'";
        if (int rc = std::system(command.c_str()); rc != 0) {
            std::cout << "\033[31m✖ failed\033[0m\n";
        } else {
            std::cout << "\033[32m✔ done\033[0m\n";
        }
    }

    std::cout << "\033[1;34m🔄 Sync complete.\033[0m\n";
    }
    else if (cmd == "remove-repo") {
        checkUID();
        // Usage: gradient remove-repo <name>
        if (args.empty()) {
            std::cerr << "\033[31merror:\033[0m 'remove-repo' requires a repository name\n";
            return;
        }
        const std::string& name = args[0];

        // Determine the repos directory
        fs::path repoBase = bootstrapDir_.empty()
            ? fs::path("/var/lib/gradient/repos")
            : fs::path(bootstrapDir_) / "var/lib/gradient/repos";

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
        std::error_code error_code;
        fs::remove(repoJson, error_code);
        if (error_code) {
            std::cerr << "\033[31merror:\033[0m failed to remove '"
                      << repoJson << "': " << error_code.message() << "\n";
            return;
        }
        std::cout << "\033[32minfo:\033[0m removed repository descriptor '"
                  << repoJson.filename() << "'\n";

        // Remove any synced data directory (<repoBase>/<name>/)
        fs::path dataDir = repoBase / name;
        if (fs::exists(dataDir)) {
            fs::remove_all(dataDir, error_code);
            if (error_code) {
                std::cerr << "\033[33mwarning:\033[0m failed to remove data directory '"
                          << dataDir << "': " << error_code.message() << "\n";
            } else {
                std::cout << "\033[32minfo:\033[0m removed repository data at '"
                          << dataDir << "'\n";
            }
        }

        std::cout << "\033[32msuccess:\033[0m repository '" << name << "' removed\n";
    }
    else if (cmd == "system-update") {
        checkUID();
        std::cout << "\033[32minfo:\033[0m system-update command invoked\n";
    }
    else if (cmd == "audit") {
        checkUID();
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
            // Usage: gradient query <pattern>
    if (args.empty()) {
        std::cerr << "\033[31merror:\033[0m 'query' requires a search pattern\n";
        return;
    }
    std::string pattern = args[0];
    std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);

    // Find repos directory
    fs::path repoBase = bootstrapDir_.empty()
        ? fs::path("/var/lib/gradient/repos")
        : fs::path(bootstrapDir_) / "var/lib/gradient/repos";

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
            auto name    = pkg["pkgname"].as<std::string>();
            std::string lowName = name;
            std::ranges::transform(lowName, lowName.begin(), ::tolower);

            if (lowName.find(pattern) == std::string::npos)
                continue;

            anyMatch = true;
            auto ver   = pkg["pkgver"].as<std::string>();
            auto arch  = pkg["arch"].as<std::string>();
            auto file  = pkg["filename"].as<std::string>();
            auto desc  = pkg["description"].as<std::string>();

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
    else if (cmd == "list") {
        // Fetch all installed packages (with broken flag)
        auto pkgs = db.listPackages();

        if (parseOutput_) {
            // machine‐friendly: name|version|arch|broken
            for (auto& p : pkgs) {
                std::cout
                  << p.name    << '|'
                  << p.version << '|'
                  << p.arch    << '|'
                  << (p.broken ? '1' : '0')
                  << "\n";
            }
        } else {
            // human‐friendly
            std::cout << "\n\033[1;34m📦 Installed Packages\033[0m\n\n";
            for (auto& p : pkgs) {
                // green check or yellow warning
                const char* sym   = p.broken ? "⚠" : "✔";
                const char* color = p.broken ? "\033[33m" : "\033[32m";

                std::cout
                  << "  "
                  << color << sym << " "
                  << "\033[1m" << p.name << "\033[0m"
                  << " \033[90m" << p.version << "\033[0m"
                  << " (" << p.arch << ")"
                  << "\033[0m\n";
            }
            std::cout << "\n";
        }
    }
    else if (cmd == "count") {
        // Fetch and print the count
        auto pkgs = db.listPackages();
        std::cout << pkgs.size() << "\n";
    }
    else {
        std::cerr << "\033[31merror:\033[0m Unknown command '" << cmd << "'\n";
    }
}

} // namespace gradient
