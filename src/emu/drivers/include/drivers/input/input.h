#pragma once

#include <common/vecx.h>
#include <cstdint>

namespace eka2l1::drivers {
    enum key_state {
        pressed,
        released,
        repeat
    };

    enum key_scancode {
        mid_button = 180,
        left_button = 196,
        right_button = 197
    };

    class input_driver {
        /**
         * \brief Get state of a button on keyboard, given the keycode.
         */
        virtual key_state get_key_state(const key_scancode key_code) = 0;

        // TODO: Touch
    };
}