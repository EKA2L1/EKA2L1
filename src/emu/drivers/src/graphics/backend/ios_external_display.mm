// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <drivers/graphics/backend/ios_external_display.h>
#include <drivers/graphics/backend/ogl/ios_gl_loader.h>
#include <common/log.h>

#import <OpenGLES/EAGL.h>
#import <QuartzCore/CAEAGLLayer.h>

#include <algorithm>
#include <deque>
#include <mutex>

namespace eka2l1::drivers {
    struct ios_external_display::impl {
        std::mutex mutex;
        CAEAGLLayer *layer = nil;
        CGSize size = CGSizeZero;
        CGFloat scale = 0;
        bool enabled = false;
        std::uint64_t revision = 0;
        std::uint64_t allocated_revision = 0;
        GLuint framebuffer = 0;
        GLuint colorbuffer = 0;
        GLint width = 0;
        GLint height = 0;
        struct frame {
            rect crop;
            vec2 surface_size;
        };
        std::deque<frame> frames;
    };

    ios_external_display::ios_external_display() : impl_(std::make_unique<impl>()) {}
    ios_external_display::~ios_external_display() = default;

    void ios_external_display::set_surface(void *surface, bool enabled) {
        CAEAGLLayer *layer = (__bridge CAEAGLLayer *)surface;
        const CGSize size = layer ? layer.bounds.size : CGSizeZero;
        const CGFloat scale = layer ? layer.contentsScale : 0;
        auto &s = *impl_;
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.layer != layer || !CGSizeEqualToSize(s.size, size) || s.scale != scale || s.enabled != enabled) {
            s.layer = layer;
            s.size = size;
            s.scale = scale;
            s.enabled = enabled;
            ++s.revision;
        }
    }

    void ios_external_display::enqueue_frame(const rect &crop, const vec2 &surface_size) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->frames.push_back({ crop, surface_size });
    }

    void ios_external_display::release() {
        auto &s = *impl_;
        if (s.framebuffer) glDeleteFramebuffers(1, &s.framebuffer);
        if (s.colorbuffer) glDeleteRenderbuffers(1, &s.colorbuffer);
        s.framebuffer = s.colorbuffer = 0;
        s.allocated_revision = 0;
    }

    void ios_external_display::present(void *context, unsigned int source_framebuffer) {
        auto &s = *impl_;
        impl::frame frame;
        bool needs_storage;
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            if (s.frames.empty()) return;
            frame = s.frames.front();
            s.frames.pop_front();
            if (!s.enabled || !s.layer || !source_framebuffer) return;
            needs_storage = s.revision != s.allocated_revision;
        }
        EAGLContext *ctx = (__bridge EAGLContext *)context;
        if (ctx.API != kEAGLRenderingAPIOpenGLES3) return;

        if (needs_storage) {
            // Never wait for UIKit while holding the mutex: disconnect and
            // background callbacks on the main thread take the same lock.
            [EAGLContext setCurrentContext:nil];
            dispatch_block_t allocate = ^{
                std::lock_guard<std::mutex> lock(s.mutex);
                if (!s.enabled || !s.layer) return;
                EAGLContext *previous = [EAGLContext currentContext];
                [EAGLContext setCurrentContext:ctx];
                GLint read_fbo, draw_fbo, renderbuffer;
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
                glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
                release();
                glGenRenderbuffers(1, &s.colorbuffer);
                glBindRenderbuffer(GL_RENDERBUFFER, s.colorbuffer);
                if ([ctx renderbufferStorage:GL_RENDERBUFFER fromDrawable:s.layer]) {
                    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &s.width);
                    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &s.height);
                    glGenFramebuffers(1, &s.framebuffer);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.framebuffer);
                    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                        GL_RENDERBUFFER, s.colorbuffer);
                    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                        s.allocated_revision = s.revision;
                    }
                }
                if (s.allocated_revision != s.revision) {
                    LOG_ERROR(DRIVER_GRAPHICS, "iOS external display drawable allocation failed");
                }
                glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
                glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
                [EAGLContext setCurrentContext:previous];
            };
            if ([NSThread isMainThread]) allocate();
            else dispatch_sync(dispatch_get_main_queue(), allocate);
            [EAGLContext setCurrentContext:ctx];
        }

        // The main-thread detach is also a barrier for all output GL work.
        std::lock_guard<std::mutex> lock(s.mutex);
        if (!s.enabled || !s.layer || s.allocated_revision != s.revision || !s.framebuffer
            || frame.crop.size.x <= 0 || frame.crop.size.y <= 0 || s.width <= 0 || s.height <= 0) return;

        GLint read_fbo, draw_fbo, renderbuffer;
        GLfloat clear_color[4];
        GLboolean color_mask[4];
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
        glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
        const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);

        const float scale = std::min(static_cast<float>(s.width) / frame.crop.size.x,
            static_cast<float>(s.height) / frame.crop.size.y);
        const int width = static_cast<int>(frame.crop.size.x * scale);
        const int height = static_cast<int>(frame.crop.size.y * scale);
        const int x = (s.width - width) / 2;
        const int y = (s.height - height) / 2;
        const int source_y = frame.surface_size.y - frame.crop.top.y - frame.crop.size.y;
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.framebuffer);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, source_framebuffer);
        glBlitFramebuffer(frame.crop.top.x, source_y,
            frame.crop.top.x + frame.crop.size.x, source_y + frame.crop.size.y,
            x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindRenderbuffer(GL_RENDERBUFFER, s.colorbuffer);
        [ctx presentRenderbuffer:GL_RENDERBUFFER];
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
        glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
        if (scissor) glEnable(GL_SCISSOR_TEST);
        glFinish();
    }
}
