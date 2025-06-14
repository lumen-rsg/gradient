//
// Created by cv2 on 6/12/25.
//

#include <iostream>
#include <unistd.h>

#include "CLI.h"

int main(int argc, char* argv[]) {
    anemo::CLI cli(argc, argv);
    cli.run();
    return 0;
}
