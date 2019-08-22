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
#include <drivers/graphics/common.h>
#include <drivers/input/common.h>
#include <drivers/graphics/texture.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace eka2l1::drivers {
    struct command_list;
    class graphics_driver;

    /** \brief Create a new bitmap in the server size.
      *
      * A bitmap will be created in the server side when using this function. When you bind
      * a bitmap for rendering, a render target will be automatically created for it.
      *
      * \param initial_size       The initial size of the bitmap.
      * 
      * \returns ID of the bitmap.
      */
    drivers::handle create_bitmap(graphics_driver *driver, const eka2l1::vec2 &size);

    /** \brief Create a new shader program.
      *
      * \param driver      The driver associated with the program. 
      * \param vert_data   Pointer to the vertex shader code.
      * \param vert_size   Size of vertex shader code.
      * \param frag_size   Size of fragment shader code.
      *
      * \returns Handle to the program.
      */
    drivers::handle create_program(graphics_driver *driver, const char *vert_data, const std::size_t vert_size,
        const char *frag_data, const std::size_t frag_size);

    /**
     * \brief Create a new texture.
     *
     * \param driver            The driver associated with the texture.
     * \param dim               Total dimensions of this texture.
     * \param mip_levels        Total mips that the data contains.
     * \param internal_format   Format stored inside GPU.
     * \param data_format       The format of the given data.
     * \param data_type         Format of the data.
     * \param data              Pointer to the data to upload.
     * \param size              Dimension size of the texture.
     *
     * \returns Handle to the texture.
     */
    drivers::handle create_texture(graphics_driver *driver, const std::uint8_t dim, const std::uint8_t mip_levels,
        drivers::texture_format internal_format, drivers::texture_format data_format, drivers::texture_format data_type,
        const void *data, const eka2l1::vec3 &size);

    struct graphics_command_list {
    };

    struct server_graphics_command_list : public graphics_command_list {
        command_list *list_;
    };

    class graphics_command_list_builder {
    protected:
        graphics_command_list *list_;

    public:
        explicit graphics_command_list_builder(graphics_command_list *list)
            : list_(list) {
        }

        virtual void set_brush_color(const eka2l1::vec3 &color) = 0;

        /**
         * \brief Set scissor rectangle, allow redraw only in specified area if invalidate is enabled.
         *
         * Use in drawing window rect or invalidate a specific region of an window.
         */
        virtual void invalidate_rect(eka2l1::rect &rect) = 0;

        /**
         * \brief Enable/disable invalidation (scissor).
         */
        virtual void set_invalidate(const bool enabled) = 0;

        /**
          * \brief Clear the binding bitmap with color.
          * \params color A RGBA vector 4 color
          */
        virtual void clear(vecx<int, 4> color) = 0;

        /**
         * \brief Set a bitmap to be current.
         *
         * Binding a bitmap results in draw operations being done within itself.
         *
         * \param handle Handle to the bitmap.
         */
        virtual void bind_bitmap(const drivers::handle handle) = 0;

        virtual void resize_bitmap(const eka2l1::vec2 &new_size) = 0;

        /**
         * \brief Update a bitmap' data region.
         *
         * The dimensions and the offset parameter must reside in bitmap boundaries create before.
         * The behavior will be undefined if these conditions are not satisfied. Though some infos will be emitted.
         *
         * \param h       The handle to existing bitmap.
         * \param data    Pointer to bitmap data.
         * \param size    Size of bitmap data.
         * \param offset  The offset of the bitmap (pixels).
         * \param dim     The dimensions of bitmap (pixels).
         * \param bpp     Number of bits per pixel.
         * 
         * \returns Handle to the texture.
         */
        virtual void update_bitmap(drivers::handle h, const int bpp, const char *data, const std::size_t size, const eka2l1::vec2 &offset,
            const eka2l1::vec2 &dim)
            = 0;

        /**
         * \brief Draw a bitmap to currently binded bitmap.
         *
         * \param h            The handle of the bitmap to blit.
         * \param dest_rect    Blit rect, specifiy the region in the given bitmap dimensions to be drawn.
         * \param use_brush    Use brush color.
         */
        virtual void draw_bitmap(drivers::handle h, const eka2l1::rect &dest_rect, const bool use_brush) = 0;
    };

    class server_graphics_command_list_builder : public graphics_command_list_builder {
        command_list *get_command_list() {
            return reinterpret_cast<server_graphics_command_list *>(list_)->list_;
        }

    public:
        explicit server_graphics_command_list_builder(graphics_command_list *list);

        void set_brush_color(const eka2l1::vec3 &color) override;

        /**
         * \brief Set scissor rectangle, allow redraw only in specified area if invalidate is enabled.
         *
         * Use in drawing window rect or invalidate a specific region of an window.
         */
        void invalidate_rect(eka2l1::rect &rect) override;

        /**
         * \brief Enable/disable invalidation (scissor).
         */
        void set_invalidate(const bool enabled) override;

        /**
          * \brief Clear the binding bitmap with color.
          * \params color A RGBA vector 4 color
          */
        void clear(vecx<int, 4> color) override;

        /**
         * \brief Set a bitmap to be current.
         *
         * Binding a bitmap results in draw operations being done within itself.
         *
         * \param handle Handle to the bitmap.
         */
        void bind_bitmap(const drivers::handle handle) override;

        void resize_bitmap(const eka2l1::vec2 &new_size) override;

        void update_bitmap(drivers::handle h, const int bpp, const char *data, const std::size_t size, const eka2l1::vec2 &offset,
            const eka2l1::vec2 &dim) override;

        void draw_bitmap(drivers::handle h, const eka2l1::rect &dest_rect, const bool use_brush) override;
    };

    /*
    class input_driver_client : public driver_client {
    public:
        input_driver_client() {}
        explicit input_driver_client(driver_instance driver);

        void lock();
        void release();

        std::uint32_t total();
        void get(input_event *evt, const std::uint32_t total_to_get);
    };*/
}

namespace eka2l1 {
}
