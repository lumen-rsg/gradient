//
// Created by cv2 on 6/12/25.
//

#ifndef SCRIPTEXECUTOR_H
#define SCRIPTEXECUTOR_H

#include <string>

namespace anemo {

    class ScriptExecutor {
    public:
        // Executes install/upgrade/remove hooks from the given script.
        // hookName should be one of: "post_common", "post_install",
        // "post_upgrade", or "post_remove".
        static void runScript(const std::string& scriptPath,
                          const std::string& hookName,
                          const std::string& chrootDir = "");
    };

} // namespace anemo

#endif //SCRIPTEXECUTOR_H
