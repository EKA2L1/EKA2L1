#pragma once

#include <drivers/instance.h>

namespace eka2l1 {
    namespace driver {
        class instance_ogl : public driver::instance {
            emu_window_ptr win_ptr;
        public:
            void init(emu_window_ptr &win) override;
        };
    }
}