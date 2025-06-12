//
// Created by cv2 on 6/12/25.
//

#include "CLI.h"
#include "cxxopts.h"
#include "Installer.h"
#include "Repository.h"
#include "Database.h"
#include <iostream>
namespace anemo {
CLI::CLI(int argc, char* argv[]) : argc_(argc), argv_(argv) {}
void CLI::run() {
    cxxopts::Options options("anemo", "anemo package manager");
    options.add_options()
        ("f,force", "force action", cxxopts::value<bool>(force_))
        ("b,bootstrap", "bootstrap directory", cxxopts::value<std::string>(bootstrapDir_))
        ("parse", "parseable output", cxxopts::value<bool>(parseOutput_))
        ("help", "help");
    options.add_options("commands")
        ("install","install pkg",cxxopts::value<std::vector<std::string>>())
        ("remove","remove pkg",cxxopts::value<std::vector<std::string>>())
        ("add-repo","add repo",cxxopts::value<std::vector<std::string>>())
        ("sync-repo","sync repo",cxxopts::value<std::vector<std::string>>())
        ("remove-repo","remove repo",cxxopts::value<std::vector<std::string>>())
        ("system-update","update system")
        ("audit","audit broken pkgs")
        ("list","list pkgs")
        ("info","info pkg",cxxopts::value<std::vector<std::string>>())
        ("query","query repo",cxxopts::value<std::vector<std::string>>());
    auto result = options.parse(argc_, argv_);
    if(result.count("help")||argc_==1){ std::cout<<options.help()<<"\n";return; }
    Database db(bootstrapDir_.empty()?"/var/lib/anemo/anemo.db":bootstrapDir_+"/anemo.db");
    db.open(); db.initSchema();
    Repository repo("",bootstrapDir_.empty()?"/var/lib/anemo/repos":bootstrapDir_+"/repos");
    if(result.count("install")) cmdInstall();
    else if(result.count("remove")) cmdRemove();
    else if(result.count("add-repo")) cmdAddRepo();
    else if(result.count("sync-repo")) cmdSyncRepo();
    else if(result.count("remove-repo")) cmdRemoveRepo();
    else if(result.count("system-update")) cmdSystemUpdate();
    else if(result.count("audit")) cmdAudit();
    else if(result.count("list")) cmdList();
    else if(result.count("info")) cmdInfo();
    else if(result.count("query")) cmdQuery();
    else std::cerr<<"Unknown command\n";
}
void CLI::cmdInstall(){std::cout<<"installing..."<<std::endl;}
void CLI::cmdRemove(){std::cout<<"removing..."<<std::endl;}
void CLI::cmdAddRepo(){std::cout<<"adding repo..."<<std::endl;}
void CLI::cmdSyncRepo(){std::cout<<"syncing repo..."<<std::endl;}
void CLI::cmdRemoveRepo(){std::cout<<"removing repo..."<<std::endl;}
void CLI::cmdSystemUpdate(){std::cout<<"updating system..."<<std::endl;}
void CLI::cmdAudit(){std::cout<<"auditing..."<<std::endl;}
void CLI::cmdList(){std::cout<<"listing packages..."<<std::endl;}
void CLI::cmdInfo(){std::cout<<"package info..."<<std::endl;}
void CLI::cmdQuery(){std::cout<<"querying repos..."<<std::endl;}
} // namespace anemo