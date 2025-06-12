//
// Created by cv2 on 6/12/25.
//

#include "Package.h"
#include "YamlParser.h"
#include <stdexcept>
namespace anemo {
    Package::Package(const std::string& archivePath):archivePath_(archivePath){}
    bool Package::loadMetadata(){
        // extract temporary and parse
        return YamlParser::parseMetadata(archivePath_+"/anemonix.yaml", meta_);
    }
    const Package::Metadata& Package::metadata() const{return meta_;}
}