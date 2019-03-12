/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <drivers/graphics/fb.h>
#include <drivers/graphics/texture.h>

#include <common/queue.h>

struct ImGuiContext;
struct ImDrawData;
struct ImDrawList;

namespace eka2l1::drivers {
    struct embed_window {
        std::uint32_t id;
        framebuffer_ptr fb;

        eka2l1::vec2 pos;
        std::uint16_t pri;

        bool visible{ false };

        explicit embed_window(const graphic_api gr_api, const eka2l1::vec2 &size, 
            const std::uint16_t pri, bool visible = false);

        bool operator<(const embed_window &rhs) {
            return pri < rhs.pri;
        }
    };

    using embed_window_ptr = std::shared_ptr<embed_window>;

    class shared_graphics_driver : public graphics_driver {
    protected:
        framebuffer_ptr framebuffer;
        ImGuiContext *context;

        ImDrawList *draw_list;

        eka2l1::cp_queue<embed_window_ptr> windows;
        std::vector<texture_ptr> bmp_textures;

        std::uint32_t id_counter = 0;
        embed_window_ptr binding;

        bool should_rerender = false;

        graphic_api gr_api_;

        virtual void redraw_window_list();
        virtual void start_new_backend_frame() {}
        virtual void render_frame(ImDrawData *draw_data) {}

    public:
        explicit shared_graphics_driver(const graphic_api gr_api, const vec2 &scr);
        ~shared_graphics_driver() override;

        void do_second_pass();
        virtual void process_requests() override;

        virtual bool do_request_queue_execute_one_request(drivers::driver_request *request);
        virtual void do_request_queue_execute_job();
        virtual void do_request_queue_clean_job() {};

        virtual drivers::handle upload_bitmap(drivers::handle h, const std::size_t size, const std::uint32_t width,
            const std::uint32_t height, const int bpp, void *data) override;

        vec2 get_screen_size() override {
            return framebuffer->get_size();
        }

        std::vector<std::uint8_t> get_render_texture_data(std::size_t stride) override {
            return framebuffer->data(stride);
        }

        std::uint32_t get_render_texture_handle() override {
            return framebuffer->texture_handle();
        }
    };
}
