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

#include <common/vecx.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#define KEY_TAB 258
#define KEY_BACKSPACE 259
#define KEY_LEFT_SHIFT 340
#define KEY_LEFT_CONTROL 341
#define KEY_LEFT_ALT 342
#define KEY_LEFT_SUPER 343
#define KEY_RIGHT_SHIFT 344
#define KEY_RIGHT_CONTROL 345
#define KEY_RIGHT_ALT 346
#define KEY_RIGHT_SUPER 347

#define KEY_RIGHT 262
#define KEY_LEFT 263
#define KEY_DOWN 264
#define KEY_UP 265

#define KEY_ENTER 257
#define KEY_ESCAPE 256

#define KEY_A 65
#define KEY_H 73
#define KEY_I 74
#define KEY_K 75
#define KEY_L 76

namespace eka2l1 {
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
        /**
         * \brief An abstract class to implement the emulator window.
         * 
         * Window are used for display graphical output. Class can base on this interface to implement
         * a new backend.
		*/
        class emu_window {
        public:
            /**
             * \brief Intialize the emulator window.
			 *
             * \param title The initial window title.
			 * \param size The initial window size.
			*/
            virtual void init(std::string title, vec2 size) = 0;

            virtual void make_current() = 0;
            virtual void done_current() = 0;
            virtual void swap_buffer() = 0;
            virtual void poll_events() = 0;
            virtual void shutdown() = 0;

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

            std::function<void(void *, vec2)> resize_hook;

            /* Callback handler */
            std::function<void(void *, point, int, int)> raw_mouse_event;

            std::function<void(void *, vec2)> mouse_wheeling;

            /*! Call when a touch input is triggered */
            std::function<void(void *, point)> touch_pressed;

            /*! Call when a touch movement is detected */
            std::function<void(void *, point)> touch_move;

            /*! Call when touch is released */
            std::function<void(void *)> touch_released;

            /*! Call when a button is pressed. User sets their own call, shutdown and center button */
            std::function<void(void *, uint16_t)> button_pressed;

            /*! Call when a button is released */
            std::function<void(void *, uint16_t)> button_released;

            /*! Call when a button is held */
            std::function<void(void *, uint16_t)> button_hold;

            /*! Call when the window is closed */
            std::function<void(void *)> close_hook;

            std::function<void(void *, char)> char_hook;
        };

        enum class window_type {
            glfw
        };

        /** \brief Create a new window emulator. */
        std::unique_ptr<emu_window> new_emu_window(window_type win_type);

        bool init_window_library(window_type win_type);
        bool destroy_window_library(window_type win_type);

        using emu_window_ptr = std::unique_ptr<emu_window>;
    }
}
