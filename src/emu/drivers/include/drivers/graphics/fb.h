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

#include <common/resource.h>
#include <common/vecx.h>

#include <drivers/graphics/common.h>

#include <memory>
#include <vector>

namespace eka2l1::drivers {
    class graphics_driver;
    class drawable;

    class framebuffer : public graphics_object {
    protected:
        std::vector<drawable *> color_buffers;
        drawable *depth_buffer;
        drawable *stencil_buffer;

    protected:
        bool is_attachment_id_valid(const std::int32_t attachment_id) const;

    public:
        /**
         * @brief Initialize a framebuffer.
         * 
         * Attachment ID of each color buffer is the index of the color buffer in the initializer list.
         * 
         * There may only exists one depth buffer or stencil buffer.
         * 
         * @param color_buffer_list                 List of color buffer to attach to this framebuffer.
         * @param depth_buffer                      Depth buffer to attach to this framebuffer.
         * @param stencil_buffer                    Stencil buffer to attach to this framebuffer
         */
        explicit framebuffer(const std::vector<drawable *> color_buffer_list, drawable *depth_buffer, drawable *stencil_buffer)
            : color_buffers(color_buffer_list)
            , depth_buffer(depth_buffer)
            , stencil_buffer(stencil_buffer) {
        }

        framebuffer() = default;
        virtual ~framebuffer(){};

        virtual void bind(graphics_driver *driver, const framebuffer_bind_type type_bind) = 0;
        virtual void unbind(graphics_driver *driver) = 0;

        virtual bool set_draw_buffer(const std::int32_t attachment_id) = 0;
        virtual bool set_read_buffer(const std::int32_t attachment_id) = 0;

        /**
         * @brief       Set the depth stencil buffer drawable of the framebuffer.
         * 
         * If a depth stencil buffer already been set to this framebuffer, it will be overriden.
         * 
         * @param       tex       The drawable to set as this framebuffer's depth stencil buffer.
         * @returns     True on success.
         */
        virtual bool set_depth_stencil_buffer(drawable *depth, drawable *stencil,
            const int depth_face_index, const int stencil_face_index) = 0;

        /**
         * @brief   Set color buffer to an attachment ID.
         * 
         * @param   tex           The color buffer drawable to attach to this framebuffer.
         * @param   position      The ID to set attachment to. -1 for API to choose.
         * @returns Attachment ID of the color buffer. -1 on failure.
         */
        virtual std::int32_t set_color_buffer(drawable *tex, const int face_index, const std::int32_t position = -1) = 0;

        /**
         * @brief       Copy content of some framebuffer to draw framebuffer.
         * 
         * @param       source_rect           The source rectangle to copy data from.
         * @param       dest_rect             The destination rectangle to put data to draw buffer.
         * @param       flags                 Contains OR of draw_buffer_bit_flags, specifying which buffers to copy from.
         * @param       copy_filter           The filter to apply to texture data copied.
         * 
         * @returns     True on success.
         */
        virtual bool blit(const eka2l1::rect &source_rect, const eka2l1::rect &dest_rect, const std::uint32_t flags,
            const filter_option copy_filter)
            = 0;

        virtual bool remove_color_buffer(const std::int32_t position) = 0;
        virtual bool read(const texture_format type, const texture_data_type dest_format, const eka2l1::point &pos, const eka2l1::object_size &size, std::uint8_t *buffer_ptr) = 0;

        virtual std::uint64_t color_attachment_handle(const std::int32_t attachment_id);
    };

    using framebuffer_ptr = std::unique_ptr<framebuffer>;

    framebuffer_ptr make_framebuffer(graphics_driver *driver, const std::vector<drawable *> &color_buffer_list,
        const std::vector<int> &face_indicies, drawable *depth_buffer, const int depth_face_index,
        drawable *stencil_buffer, const int stencil_face_index);
}
