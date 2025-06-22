//
// Created by cv2 on 6/12/25.
//

#include "ScriptExecutor.h"

#include <filesystem>
#include <cstdlib>
#include <iostream>

namespace fs = std::filesystem;
namespace gradient {
    void ScriptExecutor::runScript(const std::string& scriptPath,
                                   const std::string& hookName,
                                   const std::string& chrootDir)
    {
        // 1) Skip if no script
        if (!fs::exists(scriptPath)) {
            std::cerr << "\033[33minfo:\033[0m script '" << scriptPath
                      << "' not found; skipping hooks\n";
            return;
        }

        // 2) Determine in‐chroot path
        std::string inChrootPath = scriptPath;
        bool doChroot = !chrootDir.empty() && chrootDir != "/";
        if (doChroot) {
            // strip leading chrootDir prefix
            if (scriptPath.rfind(chrootDir, 0) == 0) {
                inChrootPath = scriptPath.substr(chrootDir.size());
                if (inChrootPath.empty()) inChrootPath = "/";
            }
        }

        // 3) Build the shell fragment that sources + hooks
        std::string inner = ". '" + inChrootPath + "'; "
            "if command -v post_common >/dev/null 2>&1; then post_common; fi; "
            "if command -v " + hookName + " >/dev/null 2>&1; then " + hookName + "; fi";

        // 4) Final command: either direct or via chroot
        std::string cmd;
        if (doChroot) {
            cmd = "chroot '" + chrootDir + "' /bin/sh -e -c \"" + inner + "\"";
        } else {
            cmd = "/bin/sh -e -c \"" + inner + "\"";
        }

        // 5) Execute it
        int rc = std::system(cmd.c_str());
        if (rc != 0) {
            std::cerr << "\033[33mwarning:\033[0m hook '" << hookName
                      << "' in script '" << scriptPath
                      << "' exited with code " << rc << "\n";
        }
    }
}