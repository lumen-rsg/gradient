//
// Created by cv2 on 6/12/25.
//

#include "TarHandler.h"
#include <cstdlib>
namespace anemo {
    bool TarHandler::extract(const std::string& archive,const std::string& dest){return std::system(("tar -xf "+archive+" -C "+dest).c_str())==0;}
    bool TarHandler::create(const std::string& sourceDir,const std::string& archive){return std::system(("tar -cf "+archive+" -C "+sourceDir+" .").c_str())==0;}
}