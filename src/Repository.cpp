//
// Created by cv2 on 6/12/25.
//

#include "Repository.h"
namespace anemo {
    Repository::Repository(const std::string& url,const std::string& localPath):url_(url),localPath_(localPath){}
    bool Repository::sync(){return true;}
    std::vector<Package::Metadata> Repository::listPackages() const{return {};}
    std::unique_ptr<Package> Repository::fetchPackage(const std::string& name,const std::string& version){
        return std::make_unique<Package>(localPath_+"/"+name+"-"+version+".apkg");}
}