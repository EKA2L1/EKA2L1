#pragma once

#include <cstdint>

namespace eka2l1::analysis {
    struct subroutine {
        bool thumb;

        std::uint32_t start;
        std::uint32_t end;
    };
}