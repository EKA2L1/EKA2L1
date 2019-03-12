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

#include <drivers/graphics/backend/graphics_driver_shared.h>
#include <common/queue.h>

struct ImGuiContext;

namespace eka2l1::drivers {
    class ogl_graphics_driver : public shared_graphics_driver {
    protected:
        virtual void start_new_backend_frame() override;
        virtual void render_frame(ImDrawData *draw_data) override;

    public:
        explicit ogl_graphics_driver(const vec2 &scr);

        bool do_request_queue_execute_one_request(drivers::driver_request *request) override;

        void do_request_queue_execute_job() override;
        void do_request_queue_clean_job() override;

        drivers::handle upload_bitmap(drivers::handle h, const std::size_t size, const std::uint32_t width,
            const std::uint32_t height, const int bpp, void *data) override;

        void set_screen_size(const vec2 &s) override;
    };
}
