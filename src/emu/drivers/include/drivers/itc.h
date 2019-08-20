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

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace eka2l1::drivers {
    struct command_list;

    /** \brief Create a new bitmap in the server size.
      *
      * A bitmap will be created in the server side when using this function. When you bind
      * a bitmap for rendering, a render target will be automatically created for it.
      *
      * \param initial_size       The initial size of the bitmap.
      * 
      * \returns ID of the bitmap.
      */
    drivers::handle create_bitmap(graphics_driver_ptr driver, const eka2l1::vec2 &size, const int bpp);

    class graphics_command_list_builder {
        command_list *list_;

    public:
        explicit graphics_command_list_builder(command_list *list);

        /**
         * \brief Set scissor rectangle, allow redraw only in specified area if invalidate is enabled.
         *
         * Use in drawing window rect or invalidate a specific region of an window.
         */
        void invalidate_rect(eka2l1::rect &rect);

        /**
         * \brief Enable/disable invalidation (scissor).
         */
        void set_invalidate(const bool enabled);

        /**
          * \brief Clear the binding bitmap with color.
          * \params color A RGBA vector 4 color
          */
        void clear(vecx<int, 4> color);

        /*! \brief Draw text with bound rect
        */
        void draw_text(eka2l1::rect rect, const std::u16string &str);

        /**
         * \brief Set a bitmap to be current.
         *
         * Binding a bitmap results in draw operations being done within itself.
         *
         * \param handle Handle to the bitmap.
         */
        void bind_bitmap(const drivers::handle handle);

        void resize_bitmap(const eka2l1::vec2 &new_size);

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
        void update_bitmap(drivers::handle h, const int bpp, const char *data, const std::size_t size, const eka2l1::vec2 &offset,
            const eka2l1::vec2 &dim);

        /**
         * \brief Draw a bitmap to currently binded bitmap.
         *
         * \param h            The handle of the bitmap to blit.
         * \param dest_rect    Blit rect, specifiy the region in the given bitmap dimensions to be drawn.
         */
        void draw_bitmap(drivers::handle h, const eka2l1::rect &dest_rect);
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
