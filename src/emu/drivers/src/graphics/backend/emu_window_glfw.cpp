/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/log.h>
#include <common/raw_bind.h>
#include <common/platform.h>

#include <drivers/graphics/backend/cursor_glfw.h>
#include <drivers/graphics/backend/emu_window_glfw.h>

#define CALL_IF_VALID(_a, ...) \
    if (_a) {                  \
        _a(__VA_ARGS__);       \
    }

void char_callback(GLFWwindow *window, unsigned int c) {
    eka2l1::drivers::emu_window_glfw3 *win = reinterpret_cast<decltype(win)>(glfwGetWindowUserPointer(window));
    CALL_IF_VALID(win->char_hook, win->get_userdata(), c);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    eka2l1::drivers::emu_window_glfw3 *win = reinterpret_cast<decltype(win)>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
        CALL_IF_VALID(win->button_pressed, win->get_userdata(), key);
    } else if (action == GLFW_RELEASE) {
        CALL_IF_VALID(win->button_released, win->get_userdata(), key);
    } else {
        CALL_IF_VALID(win->button_hold, win->get_userdata(), key);
    }
}

void mouse_callback(GLFWwindow *window, int button, int action, int mods) {
    eka2l1::drivers::emu_window_glfw3 *win = reinterpret_cast<decltype(win)>(glfwGetWindowUserPointer(window));

    double x;
    double y;
    glfwGetCursorPos(window, &x, &y);

    int but_action = (action == GLFW_PRESS) ? 0 : ((action == GLFW_REPEAT) ? 1 : 2);

    CALL_IF_VALID(win->raw_mouse_event, win->get_userdata(), eka2l1::point(static_cast<int>(x), static_cast<int>(y)),
        button - GLFW_MOUSE_BUTTON_1, but_action);

    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }

    if (action == GLFW_PRESS) {
        CALL_IF_VALID(win->touch_pressed, win->get_userdata(), eka2l1::point(static_cast<int>(x), static_cast<int>(y)));
    } else if (action == GLFW_REPEAT) {
        CALL_IF_VALID(win->touch_move, win->get_userdata(), eka2l1::point(static_cast<int>(x), static_cast<int>(y)));
    } else {
        CALL_IF_VALID(win->touch_released, win->get_userdata());
    }
}

void mouse_wheel_callback(GLFWwindow *window, double xoff, double yoff) {
    eka2l1::drivers::emu_window_glfw3 *win = reinterpret_cast<decltype(win)>(glfwGetWindowUserPointer(window));
    CALL_IF_VALID(win->mouse_wheeling, win->get_userdata(), { xoff, yoff });
}

void fb_resize_callback(GLFWwindow *window, int width, int height) {
    eka2l1::drivers::emu_window_glfw3 *win = reinterpret_cast<decltype(win)>(glfwGetWindowUserPointer(window));
    CALL_IF_VALID(win->resize_hook, win->get_userdata(), eka2l1::vec2(width, height));
}

void close_callback(GLFWwindow *window) {
    eka2l1::drivers::emu_window_glfw3 *win = reinterpret_cast<decltype(win)>(glfwGetWindowUserPointer(window));
    CALL_IF_VALID(win->close_hook, win->get_userdata());
}

static void mouse_cursor_callback(GLFWwindow *window, double xpos, double ypos) {
    eka2l1::drivers::emu_window_glfw3 *win = reinterpret_cast<decltype(win)>(glfwGetWindowUserPointer(window));

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
        CALL_IF_VALID(win->touch_move, win->get_userdata(), eka2l1::vec2(static_cast<int>(xpos), static_cast<int>(ypos)));
    }
}

namespace eka2l1 {
    namespace drivers {
        const uint32_t default_width_potrait = 360;
        const uint32_t default_height_potrait = 640;

        emu_window_glfw3::emu_window_glfw3()
            : userdata(nullptr)
            , is_fullscreen_now(false) {
        }

        bool emu_window_glfw3::get_mouse_button_hold(const int mouse_btt) {
            return (glfwGetMouseButton(emu_win, mouse_btt) != 0) ? true : false;
        }

        vec2 emu_window_glfw3::window_size() {
            vec2 v;
            glfwGetWindowSize(emu_win, &v.x, &v.y);

            return v;
        }

        vec2 emu_window_glfw3::window_fb_size() {
            vec2 v;
            glfwGetFramebufferSize(emu_win, &v.x, &v.y);

            return v;
        }

        vec2d emu_window_glfw3::get_mouse_pos() {
            vec2d v;
            glfwGetCursorPos(emu_win, &(v[0]), &(v[1]));

            return v;
        }

        bool emu_window_glfw3::should_quit() {
            return (glfwWindowShouldClose(emu_win) == GL_TRUE);
        }

        void emu_window_glfw3::init(std::string title, vec2 size, const std::uint32_t flags) {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if EKA2L1_PLATFORM(MACOS)
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

            GLFWmonitor *monitor = glfwGetPrimaryMonitor();

            if (flags & emu_window_flag_maximum_size) {
                const GLFWvidmode *mode = glfwGetVideoMode(monitor);

                size.x = mode->width;
                size.y = mode->height;

                glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
            }

            emu_win = glfwCreateWindow(size.x, size.y, title.data(), (flags & emu_window_flag_fullscreen) ? monitor : nullptr, nullptr);

            if (!emu_win) {
                // Try to create OpenGL 3.1
                const std::int32_t minor_to_try[] = { 2, 1 };
                const int mode_to_try[] = { GLFW_OPENGL_CORE_PROFILE, GLFW_OPENGL_ANY_PROFILE };

                for (std::size_t i = 0; i < sizeof(minor_to_try) / sizeof(std::int32_t); i++) {
                    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor_to_try[i]);
                    glfwWindowHint(GLFW_OPENGL_PROFILE, mode_to_try[i]);

    #if EKA2L1_PLATFORM(MACOS)
                    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    #endif

                    emu_win = glfwCreateWindow(size.x, size.y, title.data(), (flags & emu_window_flag_fullscreen) ? monitor : nullptr, nullptr);

                    if (emu_win) {
                        break;
                    }
                }
                
                if (!emu_win) {
                    LOG_ERROR(DRIVER_GRAPHICS, "Can't create window! Check if your PC support at least OpenGL 3.1!");
                    return;
                }
            }

            using namespace std::placeholders;

            glfwSetWindowUserPointer(emu_win, this);

            glfwSetKeyCallback(emu_win, &key_callback);
            glfwSetMouseButtonCallback(emu_win, &mouse_callback);
            glfwSetFramebufferSizeCallback(emu_win, &fb_resize_callback);
            glfwSetWindowCloseCallback(emu_win, &close_callback);
            glfwSetScrollCallback(emu_win, &mouse_wheel_callback);
            glfwSetCharCallback(emu_win, &char_callback);
            glfwSetCursorPosCallback(emu_win, &mouse_cursor_callback);

            emu_screen_size = size;
        }

        void emu_window_glfw3::set_fullscreen(const bool is_fullscreen) {
            if (is_fullscreen_now == is_fullscreen) {
                return;
            }

            GLFWmonitor *primary = glfwGetPrimaryMonitor();
            const GLFWvidmode *mode = glfwGetVideoMode(primary);

            glfwSetWindowMonitor(emu_win, (is_fullscreen ? primary : NULL), 0, 30,
                emu_screen_size.x, emu_screen_size.y, mode->refreshRate);

            is_fullscreen_now = is_fullscreen;
        }

        void emu_window_glfw3::change_title(const std::string &new_title) {
            glfwSetWindowTitle(emu_win, new_title.c_str());
        }

        void emu_window_glfw3::make_current() {
            glfwMakeContextCurrent(emu_win);
        }

        void emu_window_glfw3::done_current() {
            glfwMakeContextCurrent(nullptr);
        }

        void emu_window_glfw3::swap_buffer() {
            glfwSwapInterval(1);
            glfwSwapBuffers(emu_win);
        }

        void emu_window_glfw3::shutdown() {
            glfwDestroyWindow(emu_win);
        }

        void emu_window_glfw3::poll_events() {
            glfwPollEvents();
        }

        void emu_window_glfw3::set_userdata(void *new_userdata) {
            userdata = new_userdata;
        }

        void *emu_window_glfw3::get_userdata() {
            return userdata;
        }

        bool emu_window_glfw3::set_cursor(cursor *cur) {
            if (!cur) {
                return false;
            }

            cursor_glfw *cur_real = reinterpret_cast<cursor_glfw*>(cur);
            glfwSetCursor(emu_win, reinterpret_cast<GLFWcursor*>(cur_real->raw_handle()));

            return true;
        }

        void emu_window_glfw3::cursor_visiblity(const bool visi) {
            glfwSetInputMode(emu_win, GLFW_CURSOR, visi ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
        }

        bool emu_window_glfw3::cursor_visiblity() {
            return (glfwGetInputMode(emu_win, GLFW_CURSOR) != GLFW_CURSOR_HIDDEN);
        }
    }
}
