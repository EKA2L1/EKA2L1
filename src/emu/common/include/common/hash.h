#pragma once

#include <cstdint>
#include <string>

namespace eka2l1 {
    namespace common {
        // https://stackoverflow.com/questions/7968674/unexpected-collision-with-stdhash
        uint32_t hash(std::string const &s) {
            uint32_t h = 5381;

            for (auto c : s)
                h = ((h << 5) + h) + c; /* hash * 33 + c */

            return h;
        }
    }
}
