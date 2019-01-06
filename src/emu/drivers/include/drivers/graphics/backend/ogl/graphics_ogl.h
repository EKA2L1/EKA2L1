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

#include <drivers/graphics/graphics.h>

#include <drivers/graphics/backend/ogl/fb_ogl.h>
#include <drivers/graphics/backend/ogl/texture_ogl.h>

#include <common/queue.h>

struct ImGuiContext;

namespace eka2l1::drivers {
    struct ogl_window {
        std::uint32_t id;
        ogl_framebuffer fb;

        eka2l1::vec2    pos;
        std::uint16_t   pri;

        bool visible { false };

        explicit ogl_window(const eka2l1::vec2 &size, const std::uint16_t pri,
            bool visible = false);

        bool operator < (const ogl_window &rhs) {
            return pri < rhs.pri;
        }
    };

    using ogl_window_ptr = std::shared_ptr<ogl_window>;

    class ogl_graphics_driver: public graphics_driver {
        ogl_framebuffer framebuffer;
        ImGuiContext *context;

        eka2l1::cp_queue<ogl_window_ptr> windows;

        std::uint32_t id_counter = 0;
        ogl_window_ptr binding;

        bool should_rerender = false;

        void redraw_window_list();

    public:
        explicit ogl_graphics_driver(const vec2 &scr);
        ~ogl_graphics_driver() override;

        void do_second_pass();
        void process_requests() override;

        vec2 get_screen_size() override {
            return framebuffer.get_size();
        }

        void set_screen_size(const vec2 &s) override;

        std::vector<std::uint8_t> get_render_texture_data(std::size_t stride) override {
            return framebuffer.data(stride);
        }

        std::uint32_t get_render_texture_handle() override {
            return framebuffer.texture_handle();
        }
    };
}