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
#include <common/vecx.h>

namespace eka2l1::drivers {
    /**
     * \brief Bitmap is basically a texture. It can be drawn into and can be taken to draw.
     */
    struct bitmap {
    public:
        std::uint32_t id;
        framebuffer_ptr fb;
        texture_ptr tex;
        texture_ptr ds_tex;
        int bpp;

        explicit bitmap(graphics_driver *driver, const eka2l1::vec2 &size, const int initial_bpp);
        virtual ~bitmap();

        void resize(graphics_driver *driver, const eka2l1::vec2 &new_size);
        void init_fb(graphics_driver *driver);
    };

    using bitmap_ptr = std::unique_ptr<bitmap>;
    using graphics_object_instance = std::unique_ptr<graphics_object>;

    class shared_graphics_driver : public graphics_driver {
    protected:
        std::vector<bitmap_ptr> bmp_textures;
        std::vector<graphics_object_instance> graphic_objects;

        bitmap *binding;
        bitmap *get_bitmap(const drivers::handle h);

        int current_fb_height;
        eka2l1::vec2 swapchain_size;

        glm::mat4 projection_matrix;
        eka2l1::vecx<float, 4> brush_color;

        drivers::handle append_graphics_object(graphics_object_instance &instance);
        bool delete_graphics_object(const drivers::handle handle);
        graphics_object *get_graphics_object(const drivers::handle num);

        // Implementations
        void set_swapchain_size(command_helper &helper);
        void create_bitmap(command_helper &helper);
        void update_bitmap(command_helper &helper);
        void bind_bitmap(command_helper &helper);
        void destroy_bitmap(command_helper &helper);
        void set_brush_color(command_helper &helper);
        void create_program(command_helper &helper);
        void create_texture(command_helper &helper);
        void create_buffer(command_helper &helper);
        void use_program(command_helper &helper);
        void set_uniform(command_helper &helper);
        void bind_texture(command_helper &helper);
        void bind_buffer(command_helper &helper);
        void update_buffer(command_helper &helper);
        void attach_descriptors(command_helper &helper);
        void destroy_object(command_helper &helper);
        void set_filter(command_helper &helper);
        void resize_bitmap(command_helper &helper);
        void set_swizzle(command_helper &helper);
        void native_dialog(command_helper &helper);
        void set_ortho_size(command_helper &helper);

    public:
        explicit shared_graphics_driver(const graphic_api gr_api);

        ~shared_graphics_driver() override;

        void update_bitmap(drivers::handle h, const std::size_t size, const eka2l1::vec2 &offset, const eka2l1::vec2 &dim,
            const void *data, const std::size_t pixels_per_line = 0) override;

        void attach_descriptors(drivers::handle h, const int stride, const bool instance_move, const attribute_descriptor *descriptors,
            const int descriptor_count) override;

        virtual void dispatch(command *cmd);

        virtual void bind_swapchain_framebuf() = 0;
    };
}
