#include <drivers/backend/emu_window_glfw.h>
#include <common/log.h>
#include <common/raw_bind.h>

namespace eka2l1 {
    namespace driver {
        const uint32_t default_width_potrait = 360;
        const uint32_t default_height_potrait = 640;

        template <typename BindFunc, int id = 0> struct fWrapper1
        {
            static void call(GLFWwindow* win, int a, int b, int c, int d)
            {
                scoped_raw_bind<BindFunc, fWrapper1<BindFunc, id>>::get_bind()(win, a, b, c, d);
            }
        };

        template <typename BindFunc, int id = 0> struct fWrapper2
        {
            static void call(GLFWwindow* win, int a, int b, int c)
            {
                scoped_raw_bind<BindFunc, fWrapper2<BindFunc, id>>::get_bind()(win, a, b, c);
            }
        }; 
        
        template <typename BindFunc, int id = 0> struct fWrapper3
        {
            static void call(GLFWwindow* win, int a, int b)
            {
                scoped_raw_bind<BindFunc, fWrapper3<BindFunc, id>>::get_bind()(win, a, b);
            }
        };

        void emu_window_glfw3::init(std::string title, vec2 size) {
            glfwInit();

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            
            emu_win = glfwCreateWindow(size.x, size.y, title.data(), glfwGetPrimaryMonitor(), nullptr);

            if (!emu_win) {
                LOG_ERROR("Can't create emulator window!");
            }

            using namespace std::placeholders;

            auto rf1 = RAW_BIND(fWrapper1, std::bind(&emu_window_glfw3::key_callback, this, _1, _2, _3, _4, _5));
            auto rf2 = RAW_BIND(fWrapper2, std::bind(&emu_window_glfw3::mouse_callback, this, _1, _2, _3, _4));
            auto rf3 = RAW_BIND(fWrapper3, std::bind(&emu_window_glfw3::fb_resize_callback, this, _1, _2, _3));

            glfwSetKeyCallback(emu_win, rf1.get_raw_ptr());
            glfwSetMouseButtonCallback(emu_win, rf2.get_raw_ptr());
            glfwSetFramebufferSizeCallback(emu_win, rf3.get_raw_ptr());

            emu_screen_size = size;
        }

        void emu_window_glfw3::fb_resize_callback(GLFWwindow* window, int width, int height) {
            resize_hook(vec2(width, height));
        }

        void emu_window_glfw3::make_current() {
            glfwMakeContextCurrent(emu_win);
        }

        void emu_window_glfw3::done_current() {
            glfwMakeContextCurrent(nullptr);
        }

        void emu_window_glfw3::swap_buffer() {
            glfwSwapBuffers(emu_win);
        }

        void emu_window_glfw3::shutdown() {
            glfwDestroyWindow(emu_win);
            glfwTerminate();
        }

        void emu_window_glfw3::poll_events() {
            glfwPollEvents();
        }

        void emu_window_glfw3::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS) {
                button_pressed(key);
            } else if (action == GLFW_RELEASE) {
                button_released();
            } else {
                button_hold(key);
            }
        }

        void emu_window_glfw3::mouse_callback(GLFWwindow* window, int button, int action, int mods) {
            if (button != GLFW_MOUSE_BUTTON_LEFT) {
                return;
            }

            double x; double y;
            glfwGetCursorPos(window, &x, &y);

            if (action == GLFW_PRESS) {
                touch_pressed(point(x, y));
            } else if (action == GLFW_REPEAT) {
                touch_move(point(x, y));
            } else {
                touch_released();
            }
        }
    }
}