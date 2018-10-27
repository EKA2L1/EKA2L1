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
#include <functional>
#include <cstdint>
#include <memory>
#include <string>

namespace eka2l1 {
	/*! \brief Contains implementation for driver */
    namespace drivers {
		/*! \brief An abstract class to implement the emulator window.
		 *
		 * Class can override methods to implement the emulator window.
		*/
        class emu_window {
        public:
		    /*! \brief Intialize the emulator window.
			 * \param title The initial window title.
			 * \param size The initial window size.
			*/
            virtual void init(std::string title, vec2 size) = 0;
            
			virtual void make_current() = 0;
            virtual void done_current() = 0;
            virtual void swap_buffer() = 0;
            virtual void poll_events() = 0;
            virtual void shutdown() = 0;

			/*! \brief Change the window title.
			*/
            virtual void change_title(std::string) = 0;

            virtual vec2 window_size() = 0;
            virtual vec2 window_fb_size() = 0;
            virtual vec2d get_mouse_pos() = 0;

            virtual bool get_mouse_button_hold(const int mouse_btt) = 0;

            std::function<void(vec2)> resize_hook;

            /* Callback handler */
            std::function<void(point, int, int)> raw_mouse_event;

            std::function<void(vec2)> mouse_wheeling;

            /*! Call when a touch input is triggered */
            std::function<void(point)> touch_pressed;

            /*! Call when a touch movement is detected */
            std::function<void(point)> touch_move;

            /*! Call when touch is released */
            std::function<void()> touch_released;

            /*! Call when a button is pressed. User sets their own call, shutdown and center button */
            std::function<void(uint16_t)> button_pressed;

            /*! Call when a button is released */
            std::function<void(uint16_t)> button_released;

            /*! Call when a button is held */
            std::function<void(uint16_t)> button_hold;

			/*! Call when the window is closed */
            std::function<void()> close_hook;

            std::function<void(char)> char_hook;
        };

        enum class window_type {
            glfw
        };

		/*! \brief Create a new window emulator. */
        std::shared_ptr<emu_window> new_emu_window(window_type win_type);
        
        bool init_window_library(window_type win_type);
        bool destroy_window_library(window_type win_type);
    }
}
