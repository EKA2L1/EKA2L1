#pragma once

#include <cstdint>

namespace eka2l1 {
    // Symbian is 32 bit
    using address = uint32_t;

    template <typename T>
    class ptr {
        address mem_address;

    public:
        template <typename T>
        ptr(const T* ptr) {
            mem_address = static_cast<address>(ptr);
        }

        address get() {
            return mem_address;
        }
    };
}
