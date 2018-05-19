#include <cstring>
#include <iostream>
#include <string>

#include <loader/rom.h>
#include <core.h>

using namespace eka2l1;

std::string rom_path = "SYM.ROM";
eka2l1::system symsys;

bool help_printed = false;

void print_help() {
    std::cout << "Usage: Drag and drop Symbian file here, ignore missing dependencies" << std::endl;
    std::cout << "Options: " << std::endl;
    std::cout << "-rom: Specified where the ROM is located. If none is specified, the emu will look for a file SYM.ROM" << std::endl; 
    std::cout << "-h/-help: Print help" << std::endl;
}

void parse_args(int argc, char** argv) {
    for (int i = 1; i < argc - 1; i++) {
        if (strncmp(argv[i], "-rom", 4) == 0) {
            rom_path = argv[++i];
        } else if (((strncmp(argv[i], "-h", 2)) == 0 || (strncmp(argv[i], "-help", 5) == 0))
            && (!help_printed)) {
            print_help();
            help_printed = true;
        }
    }
}

void init() {
    symsys.init();
    bool res = symsys.load_rom(rom_path);
}

void shutdown() {
    symsys.shutdown();
}

int main(int argc, char** argv) {
    std::cout << "EKA2L1: Experimental Symbian Emulator" << std::endl;
    parse_args(argc, argv);

    init();

    return 0;
}