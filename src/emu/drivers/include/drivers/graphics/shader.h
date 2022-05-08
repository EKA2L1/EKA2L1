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

    enum class shader_var_type {
        none,
        integer,
        real,
        boolean,
        vec2,
        vec3,
        vec4,
        ivec2,
        ivec3,
        ivec4,
        bvec2,
        bvec3,
        bvec4,
        sampler1d,
        sampler2d,
        sampler_cube,
        mat2,
        mat3,
        mat4
    };

    static constexpr std::uint32_t TOTAL_BITS_PER_SHADER_VAR_TYPE = 5;

    struct shader_program_metadata {
        const std::uint8_t *metadata_;

    public:
        explicit shader_program_metadata() = default;
        explicit shader_program_metadata(const std::uint8_t *metadata);

        const bool is_available() const {
            return metadata_;
        }

        const std::uint16_t get_attribute_count() const;
        const std::uint16_t get_uniform_count() const;

        const std::uint16_t get_attribute_max_name_length() const;
        const std::uint16_t get_uniform_max_name_length() const;

        bool get_uniform_info(const char *name, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) const;
        bool get_attribute_info(const char *name, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) const;

        bool get_uniform_info(const int index, std::string &name, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) const;
        bool get_attribute_info(const int index, std::string &name, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) const;

        const std::int32_t get_uniform_binding(const char *name) const;
        const std::int32_t get_attribute_binding(const char *name) const;
    };

    class shader_module : public graphics_object {
    public:
        virtual ~shader_module() {
        }

        virtual bool create(graphics_driver *driver, const char *data, const std::size_t size, const shader_module_type type,
            std::string *compile_log = nullptr) = 0;
    };

    using shader_module_instance = std::shared_ptr<shader_module>;

    class shader_program: public graphics_object {
    public:
        virtual ~shader_program() {
            
        }

        virtual bool create(graphics_driver *driver, shader_module *vertex_module, shader_module *fragment_module, std::string *link_log = nullptr) = 0;
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

    std::unique_ptr<shader_module> make_shader_module(graphics_driver *driver);
    std::unique_ptr<shader_program> make_shader_program(graphics_driver *driver);
}
