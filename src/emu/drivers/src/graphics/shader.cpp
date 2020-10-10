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
    std::unique_ptr<shader> make_shader(graphics_driver *driver) {
        switch (driver->get_current_api()) {
        case graphic_api::opengl: {
            return std::make_unique<ogl_shader>();
        }

        default:
            break;
        }

        return nullptr;
    }

    shader_metadata::shader_metadata(const std::uint8_t *metadata)
        : metadata_(metadata) {
    }

    const std::uint16_t shader_metadata::get_attribute_count() const {
        return reinterpret_cast<const std::uint16_t *>(metadata_)[2];
    }

    const std::uint16_t shader_metadata::get_uniform_count() const {
        return reinterpret_cast<const std::uint16_t *>(metadata_)[3];
    }

    static std::int32_t search_binding(const std::uint8_t *metadata, const char *name, const std::uint16_t offset,
        const std::uint16_t count) {
        const std::uint8_t *data = metadata + offset;

        for (std::uint16_t i = 0; i < count; i++) {
            std::uint16_t name_len = *data;

            if (strncmp(name, reinterpret_cast<const char *>(data + 1), name_len) == 0) {
                return *reinterpret_cast<const std::int32_t*>(&data[name_len + 1]);
            }

            data += name_len + 5;
        }

        return -1;
    }

    const std::int32_t shader_metadata::get_uniform_binding(const char *name) const {
        return search_binding(metadata_, name, reinterpret_cast<const std::uint16_t *>(metadata_)[1], get_uniform_count());
    }

    const std::int32_t shader_metadata::get_attribute_binding(const char *name) const {
        return search_binding(metadata_, name, reinterpret_cast<const std::uint16_t *>(metadata_)[0], get_attribute_count());
    }
}
