//
// Created by cv2 on 6/12/25.
//

#include <iostream>
#include <unistd.h>

#include "CLI.h"

int main(int argc, char* argv[]) {

    if (geteuid() != 0) {
        std::cerr << "\033[31merror:\033[0m this operation requires root privileges\n";
        return 1;
    }

    anemo::CLI cli(argc, argv);
    cli.run();
    return 0;
}
