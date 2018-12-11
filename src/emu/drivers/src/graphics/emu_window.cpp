#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/backend/emu_window_glfw.h>

namespace eka2l1 {
    namespace drivers {
        std::shared_ptr<emu_window> new_emu_window(window_type win_type) {
            switch (win_type) {
                case window_type::glfw: {
                    return std::make_shared<emu_window_glfw3>();
                }
            }

            return std::shared_ptr<emu_window>(nullptr);
        }

        bool init_window_library(window_type win_type) {
            switch (win_type) {
                case window_type::glfw:
                    return glfwInit() == GLFW_TRUE ? true : false;

                default:
                    break;
            }

            return false;
        }

        bool destroy_window_library(window_type win_type) {
            switch (win_type) {
                case window_type::glfw:
                    glfwTerminate();
                    return true;

                default:
                    break;
            }

            return false;
        }
    }
}