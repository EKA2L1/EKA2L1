/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <drivers/graphics/backend/ogl/shader_ogl.h>
#include <drivers/graphics/graphics.h>
#include <drivers/graphics/shader.h>

#include <cstring>

namespace eka2l1::drivers {
    std::unique_ptr<shader_module> make_shader_module(graphics_driver *driver) {
        switch (driver->get_current_api()) {
        case graphic_api::opengl: {
            return std::make_unique<ogl_shader_module>();
        }

        default:
            break;
        }

        return nullptr;
    }

    std::unique_ptr<shader_program> make_shader_program(graphics_driver *driver) {
        switch (driver->get_current_api()) {
        case graphic_api::opengl: {
            return std::make_unique<ogl_shader_program>();
        }

        default:
            break;
        }

        return nullptr;
    }

    shader_program_metadata::shader_program_metadata(const std::uint8_t *metadata)
        : metadata_(metadata) {
    }

    const std::uint16_t shader_program_metadata::get_attribute_count() const {
        return reinterpret_cast<const std::uint16_t *>(metadata_)[2];
    }

    const std::uint16_t shader_program_metadata::get_uniform_count() const {
        return reinterpret_cast<const std::uint16_t *>(metadata_)[3];
    }

    const std::uint16_t shader_program_metadata::get_attribute_max_name_length() const {
        return reinterpret_cast<const std::uint16_t *>(metadata_)[4];
    }

    const std::uint16_t shader_program_metadata::get_uniform_max_name_length() const {
        return reinterpret_cast<const std::uint16_t *>(metadata_)[5];
    }

    static bool search_binding(const std::uint8_t *metadata, const char *name, const std::uint16_t offset,
        const std::uint16_t count, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) {
        const std::uint8_t *data = metadata + offset;

        for (std::uint16_t i = 0; i < count; i++) {
            std::uint16_t name_len = *data;

            if (strncmp(name, reinterpret_cast<const char *>(data + 1), strlen(name)) == 0) {
                // For ARM also. So just do this
                std::memcpy(&binding, &data[name_len + 1], sizeof(std::int32_t));
                std::memcpy(&var_type, &data[name_len + 1] + sizeof(std::int32_t), sizeof(shader_var_type));
                std::memcpy(&array_size, &data[name_len + 1] + sizeof(std::int32_t) + sizeof(shader_var_type), sizeof(std::int32_t));
                return true;
            }

            data += name_len + 1 + sizeof(std::int32_t) + sizeof(shader_var_type) + sizeof(std::int32_t);
        }

        binding = -1;
        return false;
    }
    
    static bool get_info_from_index(const std::uint8_t *metadata, const int index, const std::uint16_t offset,
        const std::uint16_t count, std::string &name, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) {
        if ((index < 0) || (static_cast<std::uint16_t>(index) >= count)) {
            binding = -1;
            return false;
        }

        std::uint16_t offset_to_binding_offset = offset - (static_cast<std::uint16_t>(count - index) * sizeof(std::uint16_t));
        std::uint16_t binding_offset = *reinterpret_cast<const std::uint16_t*>(metadata + offset_to_binding_offset);

        metadata += binding_offset;

        std::uint8_t name_len = *metadata;
        metadata++;

        name = std::string(metadata, metadata + name_len);
        metadata += name_len;

        // For ARM also. So just do this
        std::memcpy(&binding, metadata, sizeof(std::int32_t));
        std::memcpy(&var_type, metadata + 4, sizeof(shader_var_type));
        std::memcpy(&array_size, metadata + 4 + sizeof(shader_var_type), sizeof(std::int32_t));

        return true;
    }

    bool shader_program_metadata::get_uniform_info(const char *name, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) const {
        return search_binding(metadata_, name, reinterpret_cast<const std::uint16_t *>(metadata_)[1], get_uniform_count(), binding, var_type, array_size);
    }

    bool shader_program_metadata::get_attribute_info(const char *name, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) const {
        return search_binding(metadata_, name, reinterpret_cast<const std::uint16_t *>(metadata_)[0], get_attribute_count(), binding, var_type, array_size);
    }
    
    bool shader_program_metadata::get_uniform_info(const int index, std::string &name, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) const {
        return get_info_from_index(metadata_, index, reinterpret_cast<const std::uint16_t *>(metadata_)[6], get_uniform_count(), name, binding,
            var_type, array_size);
    }

    bool shader_program_metadata::get_attribute_info(const int index, std::string &name, std::int32_t &binding, shader_var_type &var_type, std::int32_t &array_size) const {
        return get_info_from_index(metadata_, index, reinterpret_cast<const std::uint16_t *>(metadata_)[1], get_attribute_count(), name, binding,
            var_type, array_size);
    }

    const std::int32_t shader_program_metadata::get_uniform_binding(const char *name) const {
        std::int32_t result_val = -1;
        drivers::shader_var_type var_type;
        std::int32_t arr_size = 1;

        get_uniform_info(name, result_val, var_type, arr_size);
        return result_val;
    }

    const std::int32_t shader_program_metadata::get_attribute_binding(const char *name) const {
        std::int32_t result_val = -1;
        drivers::shader_var_type var_type;
        std::int32_t arr_size = 1;

        get_attribute_info(name, result_val, var_type, arr_size);
        return result_val;
    }
}
