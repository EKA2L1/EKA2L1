/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#pragma once

#include <drivers/graphics/common.h>
#include <drivers/input/common.h>

#include <common/vecx.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

enum {
#define KEY_CODE(name, key_name, code) name = code,
#include <drivers/graphics/keycode.inc>
#undef KEY_CODE
};

const char *number_to_key_name(const int keycode);

namespace eka2l1 {
    static constexpr std::uint32_t MAX_SYMBIAN_SUPPORTED_POINTERS = 8;
    static constexpr std::uint32_t PRESSURE_MAX_NUM = 0x7FFFFFFF;

    /**
     * \brief Contains implementation for driver.
     * 
     * Driver is a complete structure which takes and does request belongs to a specific
     * category. For example:
     * 
     * Graphic driver has the job of taking draw requests and render it to the screen.
     * Input driver has the job of tracking input from keyboard/pad and notify it to users.
     * 
     * The concept of driver here in the emulator are mostly the same as what you heard in mordern
     * computer, think of this as another nano-driver.
     * 
     * Driver in EKA2L1 hides the API difference between multiple API backends. For example, graphic driver
     * may use Vulkan API or OpenGL API, but here those API are hidden and wrapped, the client will not know
     * the implementation of the graphic driver, what API it use, but rather just send the requests, and the
     * driver will get it done.
     */
    namespace drivers {
        enum emu_window_flag {
            emu_window_flag_fullscreen = 1 << 0,
            emu_window_flag_maximum_size = 1 << 1
        };

        class cursor;

        /**
         * \brief An abstract class to implement the emulator window.
         * 
         * Window are used for display graphical output. Class can base on this interface to implement
         * a new backend.
		*/
        class emu_window {
        public:
            virtual ~emu_window() {}

            /**
             * \brief Intialize the emulator window.
			 *
             * \param title The initial window title.
			 * \param size The initial window size.
             * \param flags Flags for window creation.
			*/
            virtual void init(std::string title, vec2 size, const std::uint32_t flags) = 0;

            // Decrapated: Will be removed when glfw is removed
            // The context should not be tied to a window anymore.
            virtual void make_current() {}
            virtual void done_current() {}
            virtual void swap_buffer() {}

            virtual void poll_events() = 0;
            virtual void shutdown() = 0;
            virtual void set_fullscreen(const bool is_fullscreen) = 0;

            virtual bool should_quit() = 0;

            /**
             * \brief Change the window title.
			*/
            virtual void change_title(std::string) = 0;

            virtual vec2 window_size() = 0;
            virtual vec2 window_fb_size() = 0;
            virtual vec2d get_mouse_pos() = 0;

            virtual bool get_mouse_button_hold(const int mouse_btt) = 0;
            virtual void set_userdata(void *userdata) = 0;
            virtual void *get_userdata() = 0;

            virtual bool set_cursor(cursor *cur) = 0;
            virtual void cursor_visiblity(const bool visi) = 0;
            virtual bool cursor_visiblity() = 0;

            virtual window_system_info get_window_system_info() {
                window_system_info temp;
                return temp;
            }

            std::function<void(void *, vec2)> resize_hook;

            /* Callback handler */
            std::function<void(void *, vec3, int, int, int)> raw_mouse_event;

            std::function<void(void *, vec2d)> mouse_wheeling;

            /*! Call when a button is pressed. User sets their own call, shutdown and center button */
            std::function<void(void *, uint32_t)> button_pressed;

            /*! Call when a button is released */
            std::function<void(void *, uint32_t)> button_released;

            /*! Call when a button is held */
            std::function<void(void *, uint32_t)> button_hold;

            /*! Call when the window is closed */
            std::function<void(void *)> close_hook;

            std::function<void(void *, std::uint32_t)> char_hook;

            std::function<void(void*)> surface_change_hook;
        };

        using emu_window_ptr = std::unique_ptr<emu_window>;
    }
}
