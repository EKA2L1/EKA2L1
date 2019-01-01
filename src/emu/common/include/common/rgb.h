#pragma once

#include <cstdint>
#include <common/vecx.h>

namespace eka2l1::common {
    inline vecx<int, 4> rgb_to_vec(const std::uint32_t rgb) {
        return vecx<int, 4> { static_cast<int>(rgb >> 24),
            static_cast<int>((rgb & 0x00FF0000) >> 16),
            static_cast<int>((rgb & 0x0000FF00) >> 8),
            static_cast<int>(rgb & 0x000000FF) };
    }
}