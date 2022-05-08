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

#include <drivers/graphics/backend/ogl/texture_ogl.h>
#include <drivers/graphics/fb.h>

#include <common/vecx.h>

#include <cstdint>

namespace eka2l1::drivers {
    class graphics_driver;

    class ogl_framebuffer : public framebuffer {
        std::uint32_t fbo;

        int last_fb{ -1 };
        framebuffer_bind_type last_bind_type;

        int max_color_attachment{ 0 };
        graphics_driver *bind_driver;

    public:
        std::uint32_t get_fbo() const {
            return fbo;
        }

        explicit ogl_framebuffer(const std::vector<drawable *> color_buffer_list, const std::vector<int> &face_index_of_color_buffers,
            drawable *depth_buffer, drawable *stencil_buffer, const int face_index_of_depth_buffer, const int face_index_of_stencil_buffer);

        ~ogl_framebuffer() override;

        void bind(graphics_driver *driver, const framebuffer_bind_type type_bind) override;
        void unbind(graphics_driver *driver) override;

        std::int32_t set_color_buffer(drawable *tex, const int face_index, const std::int32_t position = -1) override;
        bool set_depth_stencil_buffer(drawable *depth, drawable *stencil, const int depth_face_index, const int stencil_face_index) override;
        bool set_draw_buffer(const std::int32_t attachment_id) override;
        bool set_read_buffer(const std::int32_t attachment_id) override;

        bool remove_color_buffer(const std::int32_t position) override;
        bool blit(const eka2l1::rect &source_rect, const eka2l1::rect &dest_rect, const std::uint32_t flags,
            const filter_option copy_filter) override;

        bool read(const texture_format type, const texture_data_type dest_format, const eka2l1::point &pos, const eka2l1::object_size &size, std::uint8_t *buffer_ptr) override;
    };
}
