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

#include <array>
#include <cstdint>
#include <memory>

namespace eka2l1::drivers {
    class graphics_driver;

    enum drawable_type {
        DRAWABLE_TYPE_TEXTURE,
        DRAWABLE_TYPE_RENDERBUFFER
    };

    class drawable: public graphics_object {
    public:
        virtual ~drawable() = default;

        virtual void bind(graphics_driver *driver, const int binding) = 0;
        virtual void unbind(graphics_driver *driver) = 0;

        virtual vec2 get_size() const = 0;
        virtual texture_format get_format() const = 0;
        virtual int get_total_dimensions() const = 0;
        virtual std::uint64_t driver_handle() = 0;

        virtual drawable_type get_drawable_type() const = 0;
    };

    /*! \brief Base class for backend texture.
    */
    class texture : public drawable {
    public:
        texture() {}

        virtual bool create(graphics_driver *driver, const int dim, const int miplvl, const vec3 &size, const texture_format internal_format,
            const texture_format format, const texture_data_type data_type, void *data, const std::size_t data_size, const std::size_t pixels_per_line = 0,
            const std::uint32_t unpack_alignment = 4)
            = 0;

        virtual ~texture() = default;

        virtual void set_filter_minmag(const bool min, const filter_option op) = 0;
        virtual void set_addressing_mode(const addressing_direction dir, const addressing_option op) = 0;
        virtual void set_channel_swizzle(channel_swizzles swizz) = 0;
        virtual void generate_mips() = 0;
        virtual void set_max_mip_level(const std::uint32_t max_mip) = 0;
        virtual void update_data(graphics_driver *driver, const int mip_lvl, const vec3 &offset, const vec3 &size, const std::size_t byte_width,
            const texture_format data_format, const texture_data_type data_type, const void *data, const std::size_t data_size, const std::uint32_t unpack_alignment)
            = 0;
        virtual texture_data_type get_data_type() const = 0;

        drawable_type get_drawable_type() const override {
            return DRAWABLE_TYPE_TEXTURE;
        }
    };

    class renderbuffer: public drawable {
    public:
        virtual bool create(graphics_driver *driver, const vec2 &size, const texture_format format) = 0;
        virtual ~renderbuffer() = default;

        int get_total_dimensions() const override {
            return 2;
        }

        drawable_type get_drawable_type() const override {
            return DRAWABLE_TYPE_RENDERBUFFER;
        }
    };

    using texture_ptr = std::unique_ptr<texture>;
    using renderbuffer_ptr = std::unique_ptr<renderbuffer>;

    texture_ptr make_texture(graphics_driver *driver);
    renderbuffer_ptr make_renderbuffer(graphics_driver *driver);
}
