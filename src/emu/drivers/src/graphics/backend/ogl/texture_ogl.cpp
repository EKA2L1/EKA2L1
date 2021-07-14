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

#include <drivers/graphics/backend/ogl/common_ogl.h>
#include <drivers/graphics/backend/ogl/texture_ogl.h>
#include <glad/glad.h>

#include <cassert>

namespace eka2l1::drivers {
    static GLint to_gl_format(const texture_format format) {
        switch (format) {
        case texture_format::r:
            return GL_RED;

        case texture_format::r8:
            return GL_R8;

        case texture_format::rg:
            return GL_RG;

        case texture_format::rgb:
            return GL_RGB;

        case texture_format::bgr:
            return GL_BGR;

        case texture_format::bgra:
            return GL_BGRA;

        case texture_format::rgba:
            return GL_RGBA;

        case texture_format::rgba4:
            return GL_RGBA4;

        case texture_format::depth24_stencil8:
            return GL_DEPTH24_STENCIL8;

        case texture_format::depth_stencil:
            return GL_DEPTH_STENCIL;

        default:
            break;
        }

        return 0;
    }

    static GLint to_gl_data_type(const texture_data_type data_type) {
        switch (data_type) {
        case texture_data_type::ubyte:
            return GL_UNSIGNED_BYTE;

        case texture_data_type::ushort:
            return GL_UNSIGNED_SHORT;

        case texture_data_type::uint_24_8:
            return GL_UNSIGNED_INT_24_8;

        case texture_data_type::ushort_4_4_4_4:
            return GL_UNSIGNED_SHORT_4_4_4_4;

        case texture_data_type::ushort_5_6_5:
            return GL_UNSIGNED_SHORT_5_6_5;

        default:
            break;
        }

        return 0;
    }

    static GLint to_gl_tex_dim(const int dim) {
        switch (dim) {
        case 1:
            return GL_TEXTURE_1D;

        case 2:
            return GL_TEXTURE_2D;

        case 3:
            return GL_TEXTURE_3D;

        default:
            break;
        }

        return 0;
    }

    bool ogl_texture::tex(graphics_driver *driver, const bool is_first) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(pixels_per_line));

        switch (dimensions) {
        case 1:
            glTexImage1D(GL_TEXTURE_1D, mip_level, to_gl_format(internal_format), tex_size.x, 0, to_gl_format(format),
                to_gl_data_type(tex_data_type), tex_data);

            break;

        case 2:
            glTexImage2D(GL_TEXTURE_2D, mip_level, to_gl_format(internal_format), tex_size.x, tex_size.y, 0, to_gl_format(format),
                to_gl_data_type(tex_data_type), tex_data);

            break;

        case 3:
            glTexImage3D(GL_TEXTURE_3D, mip_level, to_gl_format(internal_format), tex_size.x, tex_size.y, tex_size.z, 0, to_gl_format(format),
                to_gl_data_type(tex_data_type), tex_data);

            break;

        default: {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

            unbind(driver);
            glDeleteTextures(1, &texture);

            return false;
        }
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        return true;
    }

    bool ogl_texture::create(graphics_driver *driver, const int dim, const int miplvl, const vec3 &size, const texture_format internal_format,
        const texture_format format, const texture_data_type data_type, void *data, const std::size_t ppl) {
        glGenTextures(1, &texture);

        dimensions = dim;
        tex_size = size;
        tex_data_type = data_type;
        tex_data = data;
        mip_level = miplvl;
        pixels_per_line = ppl;

        bind(driver, 0);

        this->internal_format = internal_format;
        this->format = format;

        bool res = tex(driver, true);
        unbind(driver);

        if (!res) {
            glDeleteTextures(1, &texture);
        }

        return res;
    }

    ogl_texture::~ogl_texture() {
        if (texture) {
            glDeleteTextures(1, &texture);
        }
    }

    void ogl_texture::change_size(const vec3 &new_size) {
        tex_size = new_size;
    }

    void ogl_texture::change_data(const texture_data_type data_type, void *data) {
        tex_data_type = data_type;
        tex_data = data;
    }

    void ogl_texture::change_texture_format(const texture_format format) {
        this->format = format;
    }

    void ogl_texture::set_filter_minmag(const bool min, const filter_option op) {
        bind(nullptr, 0);
        glTexParameteri(GL_TEXTURE_2D, min ? GL_TEXTURE_MIN_FILTER : GL_TEXTURE_MAG_FILTER, to_filter_option(op));
        unbind(nullptr);
    }

    static GLint translate_hal_swizzle_to_gl_swizzle(const channel_swizzle &swizz) {
        switch (swizz) {
        case channel_swizzle::red:
            return GL_RED;

        case channel_swizzle::blue:
            return GL_BLUE;

        case channel_swizzle::green:
            return GL_GREEN;

        case channel_swizzle::alpha:
            return GL_ALPHA;

        case channel_swizzle::one:
            return GL_ONE;

        case channel_swizzle::zero:
            return GL_ZERO;

        default:
            break;
        }

        assert(false && "UNREACHABLE");
        return GL_ONE;
    }

    void ogl_texture::set_channel_swizzle(const channel_swizzles &swizz) {
        GLint swizz_gl[4];

        for (int i = 0; i < 4; i++) {
            swizz_gl[i] = translate_hal_swizzle_to_gl_swizzle(swizz[i]);
        }

        bind(nullptr, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, swizz_gl[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, swizz_gl[1]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, swizz_gl[2]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, swizz_gl[3]);
        unbind(nullptr);
    }

    static GLenum get_binding_enum_dim(const int dim) {
        switch (dim) {
        case 1: {
            return GL_TEXTURE_BINDING_1D;
        }

        case 2: {
            return GL_TEXTURE_BINDING_2D;
        }

        case 3: {
            return GL_TEXTURE_BINDING_3D;
        }

        default:
            break;
        }

        return GL_TEXTURE_BINDING_2D;
    }

    void ogl_texture::bind(graphics_driver *driver, const int binding) {
        glGetIntegerv(get_binding_enum_dim(dimensions), &last_tex);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active);

        glActiveTexture(GL_TEXTURE0 + binding);
        glBindTexture(to_gl_tex_dim(dimensions), texture);
    }

    void ogl_texture::unbind(graphics_driver *driver) {
        glActiveTexture(last_active);
        glBindTexture(to_gl_tex_dim(dimensions), last_tex);
        last_tex = 0;
    }

    void ogl_texture::update_data(graphics_driver *driver, const int mip_lvl, const vec3 &offset, const vec3 &size, const std::size_t pixels_per_line,
        const texture_format data_format, const texture_data_type data_type, const void *data) {
        bind(driver, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(pixels_per_line));

        switch (dimensions) {
        case 1:
            glTexSubImage1D(GL_TEXTURE_1D, mip_lvl, offset.x, size.x, to_gl_format(data_format), to_gl_data_type(data_type), data);
            break;

        case 2: {
            glTexSubImage2D(GL_TEXTURE_2D, mip_lvl, offset.x, offset.y, size.x, size.y, to_gl_format(data_format), to_gl_data_type(data_type), data);
            break;
        }

        case 3:
            glTexSubImage3D(GL_TEXTURE_3D, mip_lvl, offset.x, offset.y, offset.z, size.x, size.y, size.z, to_gl_format(data_format),
                to_gl_data_type(data_type), data);

            break;

        default:
            break;
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        unbind(driver);
    }
}
