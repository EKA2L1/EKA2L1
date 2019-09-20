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
#include <glad/glad.h>

#include <common/log.h>

namespace eka2l1::drivers {
    ogl_framebuffer::ogl_framebuffer(texture *color_buffer, texture *depth_buffere)
        : framebuffer(color_buffer, depth_buffere) {
        glGenFramebuffers(1, &fbo);
        bind(nullptr);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            static_cast<GLuint>(color_buffer->texture_handle()),
            color_buffer->get_mip_level());

        if (depth_buffer) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                static_cast<GLuint>(depth_buffer->texture_handle()),
                depth_buffer->texture_handle());
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("Framebuffer not complete!");
        }

        glClearColor(0.2f, 0.4f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        unbind(nullptr);
    }

    ogl_framebuffer::~ogl_framebuffer() {
        glDeleteFramebuffers(1, &fbo);
    }

    void ogl_framebuffer::bind(graphics_driver *driver) {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &last_fb);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    void ogl_framebuffer::unbind(graphics_driver *driver) {
        glBindFramebuffer(GL_FRAMEBUFFER, last_fb);
        last_fb = 0;
    }

    std::uint64_t ogl_framebuffer::texture_handle() {
        return color_buffer->texture_handle();
    }
}
