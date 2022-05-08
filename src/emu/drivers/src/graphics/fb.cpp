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

#include <drivers/graphics/backend/ogl/fb_ogl.h>
#include <drivers/graphics/fb.h>
#include <drivers/graphics/graphics.h>

namespace eka2l1::drivers {
    std::uint64_t framebuffer::color_attachment_handle(const std::int32_t attachment_id) {
        if ((attachment_id < 0) || (attachment_id >= color_buffers.size())) {
            return 0;
        }

        return color_buffers[attachment_id]->driver_handle();
    }

    bool framebuffer::is_attachment_id_valid(const std::int32_t attachment_id) const {
        return !((color_buffers.size() <= attachment_id) || (color_buffers[attachment_id] == nullptr));
    }

    framebuffer_ptr make_framebuffer(graphics_driver *driver, const std::vector<drawable *> &color_buffer_list,
        const std::vector<int> &face_indicies, drawable *depth_buffer, const int depth_face_index,
        drawable *stencil_buffer, const int stencil_face_index) {
        switch (driver->get_current_api()) {
        case graphic_api::opengl: {
            return std::make_unique<ogl_framebuffer>(color_buffer_list, face_indicies, depth_buffer, stencil_buffer, depth_face_index, stencil_face_index);
            break;
        }

        default:
            break;
        }

        return nullptr;
    }
}
