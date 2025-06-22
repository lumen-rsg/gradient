//
// Created by cv2 on 6/12/25.
//

#include <iostream>
#include <unistd.h>

#include "CLI.h"

int main(const int argc, char* argv[]) {
    gradient::CLI cli(argc, argv);
    cli.run();
    return 0;
}
