#include <loader/sis_fields.h>
#include <loader/sis_script_intepreter.h>

#include <cstdio>
#include <cassert>

#include <vector>
#include <iostream>

#include <fstream>

namespace eka2l1 {
    namespace loader {
        enum {
            uid1_cst = 0x10201A7a
        };

        bool install_sis(std::string path, sis_drive drv) {
            sis_parser parser(path);

            sis_header header = parser.parse_header();
            sis_contents cs = parser.parse_contents();

            ss_interpreter it(std::make_shared<std::ifstream>(path),
                              cs.controller.install_block, cs.data, drv);

            // not install yet

            return true;
        }
    }
}
