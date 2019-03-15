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
    ogl_framebuffer::ogl_framebuffer(const vec2 &size)
        : framebuffer(size) {
        glGenFramebuffers(1, &fbo);
        bind();

        texture.create(2, 0, vec3(size.x, size.y, 0), texture_format::rgba, texture_format::rgba,
            texture_data_type::ubyte, nullptr);

        texture.set_filter_minmag(true, drivers::filter_option::linear);
        texture.set_filter_minmag(false, drivers::filter_option::linear);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 
            static_cast<GLuint>(texture.texture_handle()),
            texture.get_mip_level());

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.x, size.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_INFO("Framebuffer not complete!");
        }

        glClearColor(0.2f, 0.4f, 0.8f, 1.0f);    
        glClear(GL_COLOR_BUFFER_BIT);

        unbind();
    }

    ogl_framebuffer::~ogl_framebuffer() {
        glDeleteFramebuffers(1, &fbo);
    }

    void ogl_framebuffer::bind() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &last_fb);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    void ogl_framebuffer::unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, last_fb);
        last_fb = 0;
    }

    void ogl_framebuffer::resize(const vec2 &s) {
        size = s;
        bind();

        texture.bind();
        texture.change_size(vec3(size.x, size.y, 0));
        texture.tex();
        texture.unbind();

        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.x, size.y);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_INFO("Framebuffer not complete!");
        }

        unbind();
    }

    std::vector<std::uint8_t> ogl_framebuffer::data(std::size_t stride_pixels) {
        std::vector<std::uint8_t> data;
        data.resize(size.x * size.y);

        glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(stride_pixels));
        glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);

        return data;
    }

    std::uint64_t ogl_framebuffer::texture_handle() {
        return texture.texture_handle();
    }
}
