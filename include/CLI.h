//
// Created by cv2 on 6/12/25.
//

#ifndef CLI_H
#define CLI_H

#include <string>
#include <vector>
namespace gradient {
    class CLI {
    public:
        CLI(int argc, char* argv[]);
        void run();
    private:
        bool force_ = false;
        std::string bootstrapDir_;
        bool parseOutput_ = false;
        int argc_; char** argv_;
    };
} // namespace anemo

#endif //CLI_H
