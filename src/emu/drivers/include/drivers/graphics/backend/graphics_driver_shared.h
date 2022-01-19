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

#define GLM_FORCE_RADIANS

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

        int current_fb_width;
        int current_fb_height;
        eka2l1::vec2 swapchain_size;

        glm::mat4 projection_matrix;
        eka2l1::vecx<float, 4> brush_color;

        drivers::handle append_graphics_object(graphics_object_instance &instance);
        bool delete_graphics_object(const drivers::handle handle);
        graphics_object *get_graphics_object(const drivers::handle num);

        // Implementations
        void set_swapchain_size(command &cmd);
        void create_bitmap(command &cmd);
        void update_bitmap(command &cmd);
        void update_texture(command &cmd);
        void read_bitmap(command &cmd);
        void bind_bitmap(command &cmd);
        void destroy_bitmap(command &cmd);
        void set_brush_color(command &cmd);
        void create_module(command &cmd);
        void create_program(command &cmd);
        void create_texture(command &cmd);
        void create_buffer(command &cmd);
        void use_program(command &cmd);
        void bind_texture(command &cmd);
        void update_buffer(command &cmd);
        void attach_descriptors(command &cmd);
        void destroy_object(command &cmd);
        void set_filter(command &cmd);
        void resize_bitmap(command &cmd);
        void set_swizzle(command &cmd);
        void set_ortho_size(command &cmd);
        void set_texture_wrap(command &cmd);
        void generate_mips(command &cmd);
        void create_input_descriptors(command &cmd);
        void set_max_mip_level(command &cmd);

    public:
        explicit shared_graphics_driver(const graphic_api gr_api);

        ~shared_graphics_driver() override;

        void update_bitmap(drivers::handle h, const std::size_t size, const eka2l1::vec2 &offset, const eka2l1::vec2 &dim,
            const void *data, const std::size_t pixels_per_line = 0) override;

        virtual void dispatch(command &cmd);

        virtual void bind_swapchain_framebuf() = 0;
    };
}
