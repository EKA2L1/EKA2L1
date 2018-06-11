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
#include <memory>
#include <string>
#include <vector>

namespace eka2l1 {

    // Symbian 9.x and lower driver
    // For Symbian^3 (EPOC10), it uses ScreenPlay, that access directly to GPU
    // This is using the CPU

    // TODO: Support GL draw calls
    using pixel_line = std::vector<char>;

    namespace driver {
        class emu_window;
        using emu_window_ptr = std::shared_ptr<emu_window>;

        using render_func = std::function<void()>;

        class screen_driver {
        protected:
            emu_window_ptr emu_win;
            std::vector<render_func> funcs;

        public:
            virtual void init(emu_window_ptr win, object_size &screen_size, object_size &font_size) = 0;
            virtual void shutdown() = 0;

            virtual void blit(const std::string &text, point &where) = 0;
            virtual bool scroll_up(rect &trect) = 0;

            virtual void clear(rect &trect) = 0;

            virtual void set_pixel(const point &pos, uint8_t color) = 0;
            virtual int get_pixel(const point &pos) = 0;

            virtual void set_word(const point &pos, int word) = 0;
            virtual int get_word(const point &pos) = 0;

            virtual void set_line(const point &pos, const pixel_line &pl) = 0;
            virtual void get_line(const point &pos, pixel_line &pl) = 0;

            virtual void begin_render() = 0;
            virtual void end_render() = 0;

            virtual eka2l1::vec2 get_window_size() = 0;

            virtual void register_render_func(render_func func) {
                funcs.push_back(func);
            }

			void reset() {
                funcs.clear();
			}
        };

        enum class driver_type {
            opengl,
            vulkan,
            directx
        };

        using screen_driver_ptr = std::shared_ptr<screen_driver>;

        screen_driver_ptr new_screen_driver(driver_type dr_type);
    }
}
