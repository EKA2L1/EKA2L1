#pragma once

#include <drivers/graphics/instance.h>

namespace eka2l1 {
    namespace driver {
        class ogl_instance : public driver::instance {
            emu_window_ptr win_ptr;

        public:
            void init(emu_window_ptr &win) override;
        };
    }
}