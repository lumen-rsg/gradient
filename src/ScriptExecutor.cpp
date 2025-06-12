//
// Created by cv2 on 6/12/25.
//

#include "ScriptExecutor.h"
#include <cstdlib>
namespace anemo {
    bool ScriptExecutor::runScript(const std::string& scriptPath,const std::string& stage){ return std::system(("/bin/sh "+scriptPath+" " + stage).c_str())==0; }
}