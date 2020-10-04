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

#include <drivers/graphics/backend/ogl/fb_ogl.h>
#include <drivers/graphics/backend/ogl/common_ogl.h>
#include <glad/glad.h>

#include <common/log.h>
#include <common/platform.h>

namespace eka2l1::drivers {
    ogl_framebuffer::ogl_framebuffer(std::initializer_list<texture*> color_buffer_list, texture *depth_and_stencil_buffer)
        : framebuffer(color_buffer_list, depth_and_stencil_buffer)
        , last_bind_type(framebuffer_bind_read_draw)
        , last_fb(-1) {
        glGenFramebuffers(1, &fbo);
        bind(nullptr, framebuffer_bind_read_draw);

        // Get max color attachments
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachment);

        for (std::size_t i = 0; i < color_buffer_list.size(); i++) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                GL_TEXTURE_2D, static_cast<GLuint>(color_buffers[i]->texture_handle()),
                color_buffers[i]->get_mip_level());
        }

        if (depth_and_stencil_buffer) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                static_cast<GLuint>(depth_and_stencil_buffer->texture_handle()),
                depth_and_stencil_buffer->get_mip_level());
        }

        const GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("Framebuffer not complete!");
        }

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearStencil(0);
#ifdef EKA2L1_PLATFORM_ANDROID
        glClearDepthf(0);
#else
        glClearDepth(0);
#endif
        glClear(GL_COLOR_BUFFER_BIT | ((depth_and_stencil_buffer) ? (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) : 0));

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
    }

    void ogl_framebuffer::unbind(graphics_driver *driver) {
        if (last_fb != -1) {    
            auto enums = fb_bind_type_to_ogl_enum(last_bind_type);
            glBindFramebuffer(enums.first, last_fb);
            
            last_fb = -1;
        }
    }

    std::int32_t ogl_framebuffer::set_color_buffer(texture *tex, const std::int32_t position) {
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

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment_id, GL_TEXTURE_2D,
            static_cast<GLuint>(color_buffers[attachment_id]->texture_handle()),
            color_buffers[attachment_id]->get_mip_level());

        return attachment_id;
    }

    bool ogl_framebuffer::set_depth_stencil_buffer(texture *tex) {
        depth_and_stencil_buffer = tex;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
            static_cast<GLuint>(tex->texture_handle()), tex->get_mip_level());

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

        glBlitFramebuffer(source_rect.top.x, source_rect.top.y, source_rect.top.x + source_rect.size.x, source_rect.top.y + source_rect.size.y,
            dest_rect.top.x, dest_rect.top.y, dest_rect.top.x + dest_rect.size.x, dest_rect.top.y + dest_rect.size.y,
            blit_mask_ogl, to_filter_option(copy_filter));
        
        return true;
    }
}
