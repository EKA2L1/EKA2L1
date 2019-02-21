#pragma once

#include <common/vecx.h>
#include <cstdint>

namespace eka2l1::drivers {
    enum key_state {
        pressed,
        released,
        repeat
    };

    class input_driver {
        /**
         * \brief Get state of a button on keyboard, given the keycode.
         */
        virtual key_state get_key_state(const std::uint32_t key_code) = 0;

        // TODO: Touch
    };
}