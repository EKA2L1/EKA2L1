#pragma once

#include <drivers/emu_window.h>
#include <memory>

namespace eka2l1 {
    namespace driver {
        using emu_window_ptr = std::shared_ptr<driver::emu_window>;

        class instance {
            virtual void init(emu_window_ptr &win) = 0;
        };
    }
}