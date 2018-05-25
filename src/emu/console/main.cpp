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
	std::cout << "-sid: Specified the SID path" << std::endl;
	std::cout << "\t Follow the SID option is the major version, minor version and path" << std::endl;
	std::cout << "-h/-help: Print help" << std::endl;
	//std::cout << "-"
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
		else if ((strncmp(argv[i], "-ver", 4) == 0 || (strncmp(argv[i], "-v", 2) == 0))) {
			int ver = std::atoi(argv[++i]);

			if (ver == 6) {
				symsys.set_symbian_version_use(epocver::epoc6);
			} else {
				symsys.set_symbian_version_use(epocver::epoc9);
			}
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