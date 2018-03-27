#include <loader/sis_fields.h>

#include <cstdio>
#include <cassert>

#include <vector>
#include <iostream>

namespace eka2l1 {
    namespace loader {
        enum {
            uid1_cst = 0x10201A7a
        };

        bool install_sis(std::string path) {
            sis_parser parser(path);

            sis_header header = parser.parse_header();
            sis_contents cs = parser.parse_contents();

            return true;
        }
    }
}
