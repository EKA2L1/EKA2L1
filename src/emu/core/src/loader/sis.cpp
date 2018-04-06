#include <loader/sis_fields.h>
#include <loader/sis_script_interpreter.h>

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

        sis_contents parse_sis(std::string path) {
            sis_parser parser(path);

            sis_header header = parser.parse_header();
            sis_contents cs = parser.parse_contents();

            return cs;
        }
    }
}
