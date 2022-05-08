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
#include <drivers/graphics/backend/ogl/graphics_ogl.h>

#include <glad/glad.h>

#include <common/bytes.h>
#include <cassert>

void decompressBlockETC2(unsigned int block_part1, unsigned int block_part2, std::uint8_t *img, int width, int height, int startx, int starty);
uint32_t PVRTDecompressPVRTC(const void *compressedData, uint32_t do2bitMode, uint32_t xDim, uint32_t yDim, uint32_t doPvrtType, uint8_t *outResultImage);

namespace eka2l1::drivers {
    static GLint to_gl_tex_dim(const int dim) {
        switch (dim) {
        case 1:
            return GL_TEXTURE_1D;

        case 2:
            return GL_TEXTURE_2D;

        case 3:
            return GL_TEXTURE_3D;

        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            return GL_TEXTURE_CUBE_MAP;

        default:
            break;
        }

        return 0;
    }

    static void decode_rgb_etc2_texture(std::vector<std::uint8_t> &dest, const std::uint8_t *source, const std::int32_t width, const std::int32_t height) {
        dest.resize(3 * width * height);

        std::uint32_t block_read = 0;
        const std::uint32_t *source_u32 = reinterpret_cast<const std::uint32_t*>(source);

        for (std::int32_t y = 0; y < height / 4; y++) {
            for (std::int32_t x = 0; x < width / 4; x++) {
                std::uint32_t block_part1 = common::byte_swap(source_u32[block_read++]);
                std::uint32_t block_part2 = common::byte_swap(source_u32[block_read++]);

                decompressBlockETC2(block_part1, block_part2, dest.data(), width, height, 4 * x, 4 * y);
            } 
        }
    }

    bool ogl_texture::create(graphics_driver *driver, const int dim, const int miplvl, const vec3 &size, const texture_format internal_format,
        const texture_format format, const texture_data_type data_type, void *data, const std::size_t total_size, const std::size_t ppl,
        const std::uint32_t unpack_alignment) {
        if (!texture)
            glGenTextures(1, &texture);

        dimensions = dim;
        tex_size = size;
        tex_data_type = data_type;
        pixels_per_line = ppl;

        bind(driver, 0);

        this->internal_format = internal_format;
        this->format = format;

        bool res = true;

        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(pixels_per_line));
        glPixelStorei(GL_UNPACK_ALIGNMENT, static_cast<GLint>(unpack_alignment));

        drivers::texture_format converted_internal_format = internal_format;
        drivers::texture_format converted_format = format;
        drivers::texture_data_type converted_data_type = tex_data_type;

        std::vector<std::uint8_t> converted_data;
        if (tex_data_type == drivers::texture_data_type::compressed) {
            ogl_graphics_driver *ogl_driver = reinterpret_cast<ogl_graphics_driver*>(driver);
            if (internal_format == drivers::texture_format::etc2_rgb8 && !ogl_driver->get_supported_feature(OGL_FEATURE_SUPPORT_ETC2)) {
                converted_data_type = drivers::texture_data_type::ubyte;
                converted_internal_format = drivers::texture_format::rgb;
                converted_format = drivers::texture_format::rgb;

                decode_rgb_etc2_texture(converted_data, reinterpret_cast<std::uint8_t*>(data), size.x, size.y);

                data = converted_data.data();
            } else if (((internal_format == drivers::texture_format::pvrtc_4bppv1_rgba) || (internal_format == drivers::texture_format::pvrtc_2bppv1_rgba) || (internal_format == drivers::texture_format::pvrtc_4bppv1_rgb) ||
                (internal_format == drivers::texture_format::pvrtc_2bppv1_rgb)) && !ogl_driver->get_supported_feature(OGL_FEATURE_SUPPORT_PVRTC)) {
                converted_data_type = drivers::texture_data_type::ubyte;
                converted_internal_format = drivers::texture_format::rgba;
                converted_format = drivers::texture_format::rgba;

                std::uint32_t is_2bit = ((internal_format == drivers::texture_format::pvrtc_2bppv1_rgba) || (internal_format == drivers::texture_format::pvrtc_2bppv1_rgb));

                converted_data.resize(4 * size.x * size.y);
                PVRTDecompressPVRTC(data, is_2bit, size.x, size.y, 0, converted_data.data());

                data = converted_data.data();
            }
        }

        if (converted_data_type == drivers::texture_data_type::compressed) {
            switch (dimensions) {
            case 1:
                glCompressedTexImage1D(GL_TEXTURE_1D, miplvl, texture_format_to_gl_enum(internal_format), tex_size.x, 0, static_cast<GLsizei>(total_size), data);
                break;

            case 2:
                glCompressedTexImage2D(GL_TEXTURE_2D, miplvl, texture_format_to_gl_enum(internal_format), tex_size.x, tex_size.y, 0, static_cast<GLsizei>(total_size), data);
                break;

            case 3:
                glCompressedTexImage3D(GL_TEXTURE_3D, miplvl, texture_format_to_gl_enum(internal_format), tex_size.x, tex_size.y, tex_size.z, 0, static_cast<GLsizei>(total_size), data);
                break;

            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (dimensions - 4), miplvl, texture_format_to_gl_enum(internal_format), tex_size.x, tex_size.y, 0, static_cast<GLsizei>(total_size), data);
                break;

            default: {
                res = false;
                break;
            }
            }
        } else {
            switch (dimensions) {
            case 1:
                glTexImage1D(GL_TEXTURE_1D, miplvl, texture_format_to_gl_enum(converted_internal_format), tex_size.x, 0, texture_format_to_gl_enum(converted_format),
                    texture_data_type_to_gl_enum(converted_data_type), data);

                break;

            case 2:
                glTexImage2D(GL_TEXTURE_2D, miplvl, texture_format_to_gl_enum(converted_internal_format), tex_size.x, tex_size.y, 0, texture_format_to_gl_enum(converted_format),
                    texture_data_type_to_gl_enum(converted_data_type), data);

                break;

            case 3:
                glTexImage3D(GL_TEXTURE_3D, miplvl, texture_format_to_gl_enum(converted_internal_format), tex_size.x, tex_size.y, tex_size.z, 0, texture_format_to_gl_enum(converted_format),
                    texture_data_type_to_gl_enum(converted_data_type), data);

                break;

            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (dimensions - 4), miplvl, texture_format_to_gl_enum(converted_internal_format), tex_size.x, tex_size.y, 0, texture_format_to_gl_enum(converted_format),
                    texture_data_type_to_gl_enum(converted_data_type), data);

                break;

            default: {
                res = false;
                break;
            }
            }
        }

        if ((internal_format == drivers::texture_format::pvrtc_4bppv1_rgb) || (internal_format == drivers::texture_format::pvrtc_2bppv1_rgb)) {
            glTexParameteri(to_gl_tex_dim(dimensions), GL_TEXTURE_SWIZZLE_A, GL_ONE);
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
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

    void ogl_texture::set_filter_minmag(const bool min, const filter_option op) {
        bind(nullptr, 0);
        glTexParameteri(to_gl_tex_dim(dimensions), (min ? GL_TEXTURE_MIN_FILTER : GL_TEXTURE_MAG_FILTER), to_filter_option(op));
        unbind(nullptr);
    }

    void ogl_texture::set_addressing_mode(const addressing_direction dir, const addressing_option op) {
        bind(nullptr, 0);
        glTexParameteri(to_gl_tex_dim(dimensions), to_tex_parameter_enum(dir), to_tex_wrapping_enum(op));
        unbind(nullptr);
    }

    void ogl_texture::generate_mips() {
        bind(nullptr, 0);
        glGenerateMipmap(to_gl_tex_dim(dimensions));
        unbind(nullptr);
    }

    static GLint translate_hal_swizzle_to_gl_swizzle(channel_swizzle swizz) {
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

    void ogl_texture::set_channel_swizzle(channel_swizzles swizz) {
        GLint swizz_gl[4];

        for (int i = 0; i < 4; i++) {
            swizz_gl[i] = translate_hal_swizzle_to_gl_swizzle(swizz[i]);
        }

        GLenum bind_point = to_gl_tex_dim(dimensions);

        bind(nullptr, 0);
        glTexParameteri(bind_point, GL_TEXTURE_SWIZZLE_R, swizz_gl[0]);
        glTexParameteri(bind_point, GL_TEXTURE_SWIZZLE_G, swizz_gl[1]);
        glTexParameteri(bind_point, GL_TEXTURE_SWIZZLE_B, swizz_gl[2]);
        glTexParameteri(bind_point, GL_TEXTURE_SWIZZLE_A, swizz_gl[3]);
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

        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            return GL_TEXTURE_BINDING_CUBE_MAP;

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
        const texture_format data_format, const texture_data_type data_type, const void *data, const std::size_t data_size, const std::uint32_t alg) {
        bind(driver, 0);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(pixels_per_line));
        glPixelStorei(GL_UNPACK_ALIGNMENT, static_cast<GLint>(alg));

        drivers::texture_format converted_format = data_format;
        drivers::texture_data_type converted_data_type = data_type;

        std::vector<std::uint8_t> converted_data;
        if (data_type == drivers::texture_data_type::compressed) {
            ogl_graphics_driver *ogl_driver = reinterpret_cast<ogl_graphics_driver*>(driver);
            if (!ogl_driver->get_supported_feature(OGL_FEATURE_SUPPORT_ETC2)) {
                converted_data_type = drivers::texture_data_type::ubyte;
                converted_format = drivers::texture_format::rgb;

                decode_rgb_etc2_texture(converted_data, reinterpret_cast<const std::uint8_t*>(data), size.x, size.y);

                data = converted_data.data();
            }
        }
    
        if (converted_data_type == texture_data_type::compressed) {
            switch (dimensions) {
            case 1:
                glCompressedTexSubImage1D(GL_TEXTURE_1D, mip_lvl, offset.x, size.x, texture_format_to_gl_enum(data_format), static_cast<GLsizei>(data_size), data);
                break;

            case 2: {
                glCompressedTexSubImage2D(GL_TEXTURE_2D, mip_lvl, offset.x, offset.y, size.x, size.y, texture_format_to_gl_enum(data_format), static_cast<GLsizei>(data_size), data);
                break;
            }

            case 3:
                glCompressedTexSubImage3D(GL_TEXTURE_3D, mip_lvl, offset.x, offset.y, offset.z, size.x, size.y, size.z, texture_format_to_gl_enum(data_format),
                    static_cast<GLsizei>(data_size), data);

                break;

            default:
                break;
            }
        } else {
            switch (dimensions) {
            case 1:
                glTexSubImage1D(GL_TEXTURE_1D, mip_lvl, offset.x, size.x, texture_format_to_gl_enum(converted_format), texture_data_type_to_gl_enum(converted_data_type), data);
                break;

            case 2: {
                glTexSubImage2D(GL_TEXTURE_2D, mip_lvl, offset.x, offset.y, size.x, size.y, texture_format_to_gl_enum(converted_format), texture_data_type_to_gl_enum(converted_data_type), data);
                break;
            }

            case 3:
                glTexSubImage3D(GL_TEXTURE_3D, mip_lvl, offset.x, offset.y, offset.z, size.x, size.y, size.z, texture_format_to_gl_enum(converted_format),
                    texture_data_type_to_gl_enum(converted_data_type), data);

                break;

            default:
                break;
            }
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        unbind(driver);
    }

    void ogl_texture::set_max_mip_level(const std::uint32_t max_mip) {
        bind(nullptr, 0);
        glTexParameteri(to_gl_tex_dim(dimensions), GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(max_mip));
        unbind(nullptr);
    }
    
    void ogl_renderbuffer::bind(graphics_driver *driver, const int binding) {
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &last_renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, handle);
    }

    void ogl_renderbuffer::unbind(graphics_driver *driver) {
        glBindRenderbuffer(GL_RENDERBUFFER_BINDING, last_renderbuffer);
        last_renderbuffer = 0;
    }

    bool ogl_renderbuffer::create(graphics_driver *driver, const vec2 &size, const texture_format format) {
        if (!handle) {
            glGenRenderbuffers(1, &handle);
        }

        this->internal_format = format;
        this->tex_size = size;

        bind(nullptr, 0);
        glRenderbufferStorage(GL_RENDERBUFFER, texture_format_to_gl_enum(format), size.x, size.y);
        unbind(nullptr);

        return true;
    }

    ogl_renderbuffer::ogl_renderbuffer() {
    }

    ogl_renderbuffer::~ogl_renderbuffer() {
        if (handle) {
            glDeleteRenderbuffers(1, &handle);
        }
    }
}
