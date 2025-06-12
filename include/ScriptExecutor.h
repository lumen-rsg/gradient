//
// Created by cv2 on 6/12/25.
//

#ifndef SCRIPTEXECUTOR_H
#define SCRIPTEXECUTOR_H

#include <string>

namespace anemo {

    class ScriptExecutor {
    public:
        static bool runScript(const std::string& scriptPath, const std::string& stage);
    };

} // namespace anemo

#endif //SCRIPTEXECUTOR_H
