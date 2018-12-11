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
#include <cstdint>

namespace eka2l1::drivers {
    enum class texture_format {
        rgb,
        rgba,
        depth_stencil,
        depth24_stencil8
    };

    enum class texture_data_type {
        ubyte,
        ushort,
        uint_24_8
    };

    enum class filter_option {
        linear
    };

    /*! \brief Base class for backend texture.
    */
    class texture {
    public:
        texture() {}

        virtual bool create(const int dim, const int miplvl, const vec3 &size, const texture_format internal_format,
            const texture_format format, const texture_data_type data_type, void *data) = 0;
        
        virtual ~texture() {};

        virtual void change_size(const vec3 &new_size) = 0;
        virtual void change_data(const texture_data_type data_type, void *data) = 0;
        virtual void change_texture_format(const texture_format internal_format, const texture_format format) = 0;

        virtual void set_filter_minmag(const bool min, const filter_option op) = 0;

        virtual void bind() = 0;
        virtual void unbind() = 0;

        virtual vec2 get_size() const = 0;
        virtual texture_format get_format() const = 0;
        virtual texture_data_type get_data_type() const = 0;
        virtual int get_total_dimensions() const = 0;
        virtual void *get_data_ptr() const = 0;
        virtual int get_mip_level() const = 0;
    };
}