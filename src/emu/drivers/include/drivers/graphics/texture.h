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

    enum class texture_format : std::uint16_t {
        none,
        r,
        rg,
        rgb,
        bgr,
        bgra,
        rgba,
        depth_stencil,
        depth24_stencil8
    };

    enum class texture_data_type : std::uint16_t {
        ubyte,
        ushort,
        ushort_5_6_5,
        uint_24_8
    };

    enum class filter_option {
        linear
    };

    enum class channel_swizzle: std::uint32_t {
        red,
        green,
        blue,
        alpha,
        zero,
        one
    };

    using channel_swizzles = std::array<channel_swizzle, 4>;

    /*! \brief Base class for backend texture.
    */
    class texture : public graphics_object {
    public:
        texture() {}

        virtual bool create(graphics_driver *driver, const int dim, const int miplvl, const vec3 &size, const texture_format internal_format,
            const texture_format format, const texture_data_type data_type, void *data)
            = 0;

        virtual ~texture(){};
        virtual std::uint64_t texture_handle() = 0;

        virtual bool tex(graphics_driver *driver, const bool is_first) = 0;

        virtual void change_size(const vec3 &new_size) = 0;
        virtual void change_data(const texture_data_type data_type, void *data) = 0;
        virtual void change_texture_format(const texture_format format) = 0;

        virtual void set_filter_minmag(const bool min, const filter_option op) = 0;
        virtual void set_channel_swizzle(channel_swizzles swizz) = 0;

        virtual void bind(graphics_driver *driver, const int binding) = 0;
        virtual void unbind(graphics_driver *driver) = 0;

        virtual vec2 get_size() const = 0;
        virtual texture_format get_format() const = 0;
        virtual texture_data_type get_data_type() const = 0;
        virtual int get_total_dimensions() const = 0;
        virtual void *get_data_ptr() const = 0;
        virtual int get_mip_level() const = 0;

        virtual void update_data(graphics_driver *driver, const int mip_lvl, const vec3 &offset, const vec3 &size, const texture_format data_format,
            const texture_data_type data_type, const void *data)
            = 0;
    };

    using texture_ptr = std::unique_ptr<texture>;

    texture_ptr make_texture(graphics_driver *driver);
}
