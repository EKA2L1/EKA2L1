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

#include <drivers/graphics/fb.h>
#include <drivers/graphics/graphics.h>
#include <drivers/graphics/texture.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <common/queue.h>

namespace eka2l1::drivers {
    /**
     * \brief Bitmap is basically a texture. It can be drawn into and can be taken to draw.
     */
    struct bitmap {
        std::uint32_t id;
        framebuffer_ptr fb;
        texture_ptr tex;
        int bpp;

        explicit bitmap(graphics_driver *driver, const eka2l1::vec2 &size, const int initial_bpp);
    };

    using bitmap_ptr = std::unique_ptr<bitmap>;

    class shared_graphics_driver : public graphics_driver {
    protected:
        std::vector<bitmap_ptr> bmp_textures;

        bitmap *binding;
        bitmap *get_bitmap(const drivers::handle h);

        glm::mat4 projection_matrix;

        // Implementations
        void create_bitmap(command_helper &helper);
        void update_bitmap(command_helper &helper);
        void bind_bitmap(command_helper &helper);
        void destroy_bitmap(command_helper &helper);

    public:
        explicit shared_graphics_driver() = default;
        explicit shared_graphics_driver(const graphic_api gr_api);

        ~shared_graphics_driver() override;

        void update_bitmap(drivers::handle h, const std::size_t size, const eka2l1::vec2 &offset, const eka2l1::vec2 &dim,
            const int bpp, const void *data) override;

        virtual void dispatch(command *cmd);
    };
}
