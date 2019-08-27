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

#include <drivers/graphics/common.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace eka2l1::drivers {
    class graphics_driver;

    enum class shader_set_var_type {
        integer,
        vec3,
        vec4,
        mat4
    };

    struct shader_metadata {
        const std::uint8_t *metadata_;

    public:
        explicit shader_metadata(const std::uint8_t *metadata);
        
        const bool is_available() const {
            return metadata_;
        }

        const std::uint16_t get_attribute_count() const;
        const std::uint16_t get_uniform_count() const;

        const std::int8_t get_uniform_binding(const char *name) const;
        const std::int8_t get_attribute_binding(const char *name) const;
    };

    class shader : public graphics_object {
    public:
        virtual bool create(graphics_driver *driver, const char *vert_data, const std::size_t vert_size,
            const char *frag_data, const std::size_t frag_size)
            = 0;

        virtual bool set(graphics_driver *driver, const int binding, const shader_set_var_type var_type, const void *data) = 0;

        virtual bool use(graphics_driver *driver) = 0;
        virtual std::optional<int> get_uniform_location(const std::string &name) = 0;
        virtual std::optional<int> get_attrib_location(const std::string &name) = 0;

        /**
         * \brief Build a metadata and get a pointer to it.
         *
         * Metadata contains location of uniforms and attributes. Some graphics API may provide it, some don't.
         */
        virtual void *get_metadata() {
            return nullptr;
        }
    };

    std::unique_ptr<shader> make_shader(graphics_driver *driver);
}
