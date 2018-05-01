#pragma once

#include <loader/sis_fields.h>

#include <string>
#include <vector>

#include <cstdint>

namespace eka2l1 {
    namespace loader {
        sis_contents parse_sis(std::string path);
    }
}
