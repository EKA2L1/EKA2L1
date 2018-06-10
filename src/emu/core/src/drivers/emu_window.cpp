#include <drivers/emu_window.h>
#include <drivers/backend/emu_window_glfw.h>

namespace eka2l1 {
    namespace driver {
        std::shared_ptr<emu_window> new_emu_window(window_type win_type) {
            switch (win_type) {
                case window_type::glfw: {
                    return std::make_shared<emu_window_glfw3>();
                }
            }

            return std::shared_ptr<emu_window>(nullptr);
        }
    }
}