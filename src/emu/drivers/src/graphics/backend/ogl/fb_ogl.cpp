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
#include <drivers/graphics/backend/ogl/fb_ogl.h>
#include <glad/glad.h>

#include <common/log.h>
#include <common/platform.h>

namespace eka2l1::drivers {
    ogl_framebuffer::ogl_framebuffer(const std::vector<drawable *> color_buffer_list, const std::vector<int> &face_index_of_color_buffers,
            drawable *depth_buffer, drawable *stencil_buffer, const int face_index_of_depth_buffer, const int face_index_of_stencil_buffer)
        : framebuffer(color_buffer_list, depth_buffer, stencil_buffer)
        , last_bind_type(framebuffer_bind_read_draw)
        , last_fb(-1)
        , bind_driver(nullptr) {
        glGenFramebuffers(1, &fbo);
        bind(nullptr, framebuffer_bind_read_draw);

        // Get max color attachments
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachment);

        for (std::size_t i = 0; i < color_buffer_list.size(); i++) {
            if (color_buffers[i]->get_drawable_type() == DRAWABLE_TYPE_RENDERBUFFER) {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                    GL_RENDERBUFFER, static_cast<GLuint>(color_buffers[i]->driver_handle()));
            } else {
                ogl_texture *otex = reinterpret_cast<ogl_texture*>(color_buffers[i]);

                glFramebufferTexture2D(GL_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                    ((otex->get_total_dimensions() > 3) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index_of_color_buffers[i] : GL_TEXTURE_2D)
                    , static_cast<GLuint>(color_buffers[i]->driver_handle()), 0);
            }
        }

        if (depth_buffer == stencil_buffer) {
            if (depth_buffer) {
                if (depth_buffer->get_drawable_type() == DRAWABLE_TYPE_RENDERBUFFER) {
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                        static_cast<GLuint>(depth_buffer->driver_handle()));
                } else {
                    ogl_texture *odepth = reinterpret_cast<ogl_texture*>(depth_buffer);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, ((odepth->get_total_dimensions() > 3)
                        ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index_of_depth_buffer : GL_TEXTURE_2D),
                        static_cast<GLuint>(depth_buffer->driver_handle()), 0);
                }
            }
        } else {
            if (depth_buffer) {
                if (depth_buffer->get_drawable_type() == DRAWABLE_TYPE_RENDERBUFFER) {
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                        static_cast<GLuint>(depth_buffer->driver_handle()));
                } else {
                    ogl_texture *odepth = reinterpret_cast<ogl_texture*>(depth_buffer);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, ((odepth->get_total_dimensions() > 3)
                        ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index_of_depth_buffer : GL_TEXTURE_2D),
                        static_cast<GLuint>(depth_buffer->driver_handle()), 0);
                }
            }
            
            if (stencil_buffer) {
                if (stencil_buffer->get_drawable_type() == DRAWABLE_TYPE_RENDERBUFFER) {
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                        static_cast<GLuint>(stencil_buffer->driver_handle()));
                } else {
                    ogl_texture *ostencil = reinterpret_cast<ogl_texture*>(stencil_buffer);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, ((ostencil->get_total_dimensions() > 3)
                        ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index_of_stencil_buffer : GL_TEXTURE_2D),
                        static_cast<GLuint>(depth_buffer->driver_handle()), 0);
                }
            }
        }

        const GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR(DRIVER_GRAPHICS, "Framebuffer not complete!");
        }

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearStencil(0);
#ifdef EKA2L1_PLATFORM_ANDROID
        glClearDepthf(0);
#else
        glClearDepth(0);
#endif
        std::uint32_t clear_flag = GL_COLOR_BUFFER_BIT;
        if (depth_buffer) clear_flag |= GL_DEPTH_BUFFER_BIT;
        if (stencil_buffer) clear_flag |= GL_STENCIL_BUFFER_BIT;

        glClear(clear_flag);

        unbind(nullptr);
    }

    ogl_framebuffer::~ogl_framebuffer() {
        glDeleteFramebuffers(1, &fbo);
    }

    static std::pair<GLenum, GLenum> fb_bind_type_to_ogl_enum(const framebuffer_bind_type type) {
        switch (type) {
        case framebuffer_bind_draw:
            return { GL_DRAW_FRAMEBUFFER, GL_DRAW_FRAMEBUFFER_BINDING };

        case framebuffer_bind_read:
            return { GL_READ_FRAMEBUFFER, GL_READ_FRAMEBUFFER_BINDING };

        case framebuffer_bind_read_draw:
            return { GL_FRAMEBUFFER, GL_FRAMEBUFFER_BINDING };

        default:
            break;
        }

        return { GL_FRAMEBUFFER, GL_FRAMEBUFFER_BINDING };
    }

    void ogl_framebuffer::bind(graphics_driver *driver, const framebuffer_bind_type type_bind) {
        auto enums = fb_bind_type_to_ogl_enum(type_bind);
        glGetIntegerv(enums.second, &last_fb);
        glBindFramebuffer(enums.first, fbo);

        last_bind_type = type_bind;
        driver = driver;
    }

    void ogl_framebuffer::unbind(graphics_driver *driver) {
        if (last_fb != -1) {
            auto enums = fb_bind_type_to_ogl_enum(last_bind_type);
            glBindFramebuffer(enums.first, last_fb);

            last_fb = -1;
        }

        bind_driver = nullptr;
    }

    std::int32_t ogl_framebuffer::set_color_buffer(drawable *tex, const int face_index, const std::int32_t position) {
        std::int32_t attachment_id = position;

        if (position == -1) {
            if (color_buffers.size() + 1 > max_color_attachment) {
                bool found = false;

                for (std::size_t i = 0; i < color_buffers.size(); i++) {
                    if (color_buffers[i] == nullptr) {
                        attachment_id = static_cast<std::int32_t>(i);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    return -1;
                }
            } else {
                attachment_id = static_cast<std::int32_t>(color_buffers.size());
            }
        }

        if (attachment_id == color_buffers.size()) {
            color_buffers.push_back(tex);
        } else {
            color_buffers[attachment_id] = tex;
        }

        if (tex->get_drawable_type() == DRAWABLE_TYPE_RENDERBUFFER) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment_id, GL_RENDERBUFFER,
                static_cast<GLuint>(tex->driver_handle()));
        } else {
            ogl_texture *ocolour = reinterpret_cast<ogl_texture*>(color_buffers[attachment_id]);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment_id, ((ocolour->get_total_dimensions() > 3) ?
                (GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index) : GL_TEXTURE_2D), static_cast<GLuint>(ocolour->driver_handle()), 0);
        }

        return attachment_id;
    }

    bool ogl_framebuffer::set_depth_stencil_buffer(drawable *depth, drawable *stencil, const int depth_face_index, const int stencil_face_index) {
        if ((depth != depth_buffer) && (depth == stencil)) {
            depth_buffer = depth;
            stencil_buffer = stencil;

            if (depth_buffer->get_drawable_type() == DRAWABLE_TYPE_RENDERBUFFER) {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                    static_cast<GLuint>(depth_buffer->driver_handle()));
            } else {
                ogl_texture *odepth = reinterpret_cast<ogl_texture*>(depth_buffer);

                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, ((odepth->get_total_dimensions() > 3) ?
                    (GL_TEXTURE_CUBE_MAP_POSITIVE_X + depth_face_index) : GL_TEXTURE_2D), static_cast<GLuint>(depth_buffer->driver_handle()), 0);
            }
        } else {
            if (depth != depth_buffer) {
                depth_buffer = depth;

                if (depth_buffer->get_drawable_type() == DRAWABLE_TYPE_RENDERBUFFER) {
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                        static_cast<GLuint>(depth_buffer->driver_handle()));
                } else {
                    ogl_texture *odepth = reinterpret_cast<ogl_texture*>(depth_buffer);

                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, ((odepth->get_total_dimensions() > 3) ?
                        (GL_TEXTURE_CUBE_MAP_POSITIVE_X + depth_face_index) : GL_TEXTURE_2D), static_cast<GLuint>(depth_buffer->driver_handle()), 0);
                }
            }
            
            if (stencil != stencil_buffer) {
                stencil_buffer = stencil;

                if (stencil_buffer->get_drawable_type() == DRAWABLE_TYPE_RENDERBUFFER) {
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                        static_cast<GLuint>(stencil_buffer->driver_handle()));
                } else {
                    ogl_texture *ostencil = reinterpret_cast<ogl_texture*>(stencil_buffer);

                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, ((ostencil->get_total_dimensions() > 3) ?
                        (GL_TEXTURE_CUBE_MAP_POSITIVE_X + depth_face_index) : GL_TEXTURE_2D),
                        static_cast<GLuint>(stencil_buffer->driver_handle()), 0);
                }
            }
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            return false;
        }

        return true;
    }

    bool ogl_framebuffer::remove_color_buffer(const std::int32_t position) {
        if (!is_attachment_id_valid(position)) {
            return false;
        }

        color_buffers[position] = nullptr;
        return true;
    }

    bool ogl_framebuffer::set_draw_buffer(const std::int32_t attachment_id) {
        if (!is_attachment_id_valid(attachment_id)) {
            return false;
        }

#ifdef EKA2L1_PLATFORM_ANDROID
        GLenum draw_buffers[2] = { GL_NONE, GL_NONE };
        draw_buffers[attachment_id] = GL_COLOR_ATTACHMENT0 + attachment_id;
        glDrawBuffers(2, draw_buffers);
#else
        glDrawBuffer(GL_COLOR_ATTACHMENT0 + attachment_id);
#endif
        return true;
    }

    bool ogl_framebuffer::set_read_buffer(const std::int32_t attachment_id) {
        if (!is_attachment_id_valid(attachment_id)) {
            return false;
        }

        glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment_id);
        return true;
    }

    bool ogl_framebuffer::blit(const eka2l1::rect &source_rect, const eka2l1::rect &dest_rect, const std::uint32_t flags,
        const filter_option copy_filter) {
        std::uint32_t blit_mask_ogl = 0;

        if (flags & draw_buffer_bit_color_buffer) {
            blit_mask_ogl |= GL_COLOR_BUFFER_BIT;
        }

        if (flags & draw_buffer_bit_depth_buffer) {
            blit_mask_ogl |= GL_DEPTH_BUFFER_BIT;
        }

        if (flags & draw_buffer_bit_stencil_buffer) {
            blit_mask_ogl |= GL_STENCIL_BUFFER_BIT;
        }

        GLboolean scissor_was_enabled = glIsEnabled(GL_SCISSOR_TEST);

        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(source_rect.top.x, source_rect.top.y, source_rect.top.x + source_rect.size.x, source_rect.top.y + source_rect.size.y,
            dest_rect.top.x, dest_rect.top.y, dest_rect.top.x + dest_rect.size.x, dest_rect.top.y + dest_rect.size.y,
            blit_mask_ogl, to_filter_option(copy_filter));

        if (scissor_was_enabled) {
            glEnable(GL_SCISSOR_TEST);
        }

        return true;
    }

    bool ogl_framebuffer::read(const texture_format type, const texture_data_type dest_format, const eka2l1::point &pos, const eka2l1::object_size &size, std::uint8_t *buffer_ptr) {
        if ((type != texture_format::rgb) && (type != texture_format::rgba) && (type != texture_format::rgba4)) {
            LOG_ERROR(DRIVER_GRAPHICS, "Framebuffer read only supports RGB/RGBA/RGBA4 (got format={})", static_cast<int>(type));
            return false;
        }

        if (((type == texture_format::rgba4) && (dest_format != texture_data_type::ushort_4_4_4_4)) ||
            ((type == texture_format::rgb) && (dest_format != texture_data_type::ushort_5_6_5)) ||
            ((type != texture_format::rgba4) && (dest_format == texture_data_type::ushort_4_4_4_4)) ||
            ((type != texture_format::rgb) && (dest_format == texture_data_type::ushort_5_6_5))) {
            LOG_ERROR(DRIVER_GRAPHICS, "Conflicted read back type/format!");
            return false;
        }
        
        GLuint format_gl = texture_data_type_to_gl_enum(dest_format);
        GLuint type_gl = texture_format_to_gl_enum(type);

        GLint read_type = 0;
        GLint read_format = 0;

        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &read_type);
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &read_format);

        if ((read_format == format_gl) && (read_type == type_gl)) {
            glReadPixels(pos.x, pos.y, size.x, size.y, type_gl, format_gl, buffer_ptr);
            return true;
        } else {
            // Read RGBA than do manual conversion. Isn't this just too cruel!!
            std::vector<std::uint8_t> temp_data;
            temp_data.resize(size.x * 4 * size.y);

            glReadPixels(pos.x, pos.y, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, temp_data.data());
    
            switch (dest_format) {
            case texture_data_type::ushort_4_4_4_4: {
                for (int y = 0; y < size.y; y++) {
                    std::uint16_t *ptr = reinterpret_cast<std::uint16_t*>(buffer_ptr + (y * (((size.x * 2) + 3) >> 2) << 2));
                    std::uint32_t *ptr_source = reinterpret_cast<std::uint32_t*>(temp_data.data() + y * size.x * 4);

                    for (int x = 0; x < size.x; x++) {
                        *ptr = ((((*ptr_source & 0xFF) / 17) & 0xF) << 8) | (((((*ptr_source >> 24) & 0xFF) / 17) & 0xF) << 12)
                            | (((((*ptr_source >> 8) & 0xFF) / 17) & 0xF) << 4) | ((((*ptr_source >> 16) & 0xFF) / 17) & 0xF);
                    
                        ptr++;
                        ptr_source++;
                    }
                }

                return true;
            }

            case texture_data_type::ushort_5_6_5: {
                for (int y = 0; y < size.y; y++) {
                    std::uint16_t *ptr = reinterpret_cast<std::uint16_t*>(buffer_ptr + (y * (((size.x * 2) + 3) >> 2) << 2));
                    std::uint32_t *ptr_source = reinterpret_cast<std::uint32_t*>(temp_data.data() + y * size.x * 4);

                    for (int x = 0; x < size.x; x++) {
                        // In order: R, G, B
                        *ptr = (((*ptr_source & 0xFF) & 0xF8) << 8) | ((((*ptr_source >> 8) & 0xFF) & 0xFC) << 3) |
                            ((((*ptr_source >> 16) & 0xFF) & 0xF8) >> 3);

                        ptr++;
                        ptr_source++;
                    }
                }

                return true;
            }

            case texture_data_type::ubyte: {
                // Reorder the data
                std::uint32_t bytes_per_pixel = (type == texture_format::rgb) ? 3 : 4;

                for (int y = 0; y < size.y; y++) {
                    std::uint8_t *ptr = buffer_ptr + (y * (((size.x * bytes_per_pixel) + 3) >> 2) << 2);
                    std::uint8_t *ptr_source = temp_data.data() + y * size.x * 4;

                    for (int x = 0; x < size.x; x++) {
                        // In order: R, G, B
                        ptr[0] = ptr_source[0];
                        ptr[1] = ptr_source[1];
                        ptr[2] = ptr_source[2];

                        if (bytes_per_pixel == 4) {
                            ptr[3] = ptr_source[3];
                        }

                        ptr += bytes_per_pixel;
                        ptr_source += 4;
                    }
                }

                return true;
            }

            default:
                LOG_ERROR(DRIVER_GRAPHICS, "Unsupported format for read conversion {}", static_cast<int>(dest_format));
                break;
            }
        }

        return false;
    }
}
