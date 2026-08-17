// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "context_eagl.h"

#include <common/log.h>
#include <drivers/graphics/backend/ogl/ios_gl_loader.h>

#import <OpenGLES/EAGL.h>
#import <QuartzCore/CAEAGLLayer.h>

namespace eka2l1::drivers::graphics {
    gl_context_eagl::gl_context_eagl(const window_system_info &wsi, bool /*stereo*/, bool /*core*/) {
        m_opengl_mode = mode::opengl_es;

        m_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
        if (!m_context) {
            // Older simulators may not advertise GLES3; fall back to GLES2 so
            // the smoke / link path still passes. Real rendering needs GLES3.
            m_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
            if (!m_context) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "EAGLContext creation failed");
                return;
            }
        }

        [EAGLContext setCurrentContext:m_context];

        CAEAGLLayer *layer = (__bridge CAEAGLLayer *)wsi.render_surface;
        if (!layer) {
            // Frontend may attach the layer later via update_surface; just
            // create the FBO so the GL backend can use it for offscreen work.
            glGenFramebuffers(1, &m_framebuffer);
            return;
        }

        attach_layer(layer);
    }

    gl_context_eagl::~gl_context_eagl() {
        release_renderbuffers();

        if (m_framebuffer != 0) {
            glDeleteFramebuffers(1, &m_framebuffer);
            m_framebuffer = 0;
        }

        if (m_context && [EAGLContext currentContext] == m_context) {
            [EAGLContext setCurrentContext:nil];
        }
        m_context = nil;
        m_layer = nil;
    }

    bool gl_context_eagl::is_headless() const {
        return m_layer == nil;
    }

    std::unique_ptr<gl_context> gl_context_eagl::create_shared_context() {
        // Shared GL contexts on iOS need an EAGLSharegroup; not used by the
        // current frontend (rendering and resource upload share one thread).
        return nullptr;
    }

    bool gl_context_eagl::make_current() {
        if (!m_context) {
            return false;
        }
        return [EAGLContext setCurrentContext:m_context] == YES;
    }

    bool gl_context_eagl::clear_current() {
        return [EAGLContext setCurrentContext:nil] == YES;
    }

    void gl_context_eagl::swap_buffers() {
        if (m_paused || !m_context || m_colorbuffer == 0) {
            return;
        }
        glBindRenderbuffer(GL_RENDERBUFFER, m_colorbuffer);
        [m_context presentRenderbuffer:GL_RENDERBUFFER];
    }

    void gl_context_eagl::update(std::uint32_t new_width, std::uint32_t new_height) {
        m_backbuffer_width = new_width;
        m_backbuffer_height = new_height;

        if (m_layer == nil) {
            return;
        }

        // The renderbuffer storage is sized from the layer's drawable; re-bind
        // it so the next presentRenderbuffer: picks up the new dimensions.
        attach_layer(m_layer);
    }

    void gl_context_eagl::set_swap_interval(std::int32_t /*interval*/) {
        // CAEAGLLayer presentation is implicitly vsynced; no API to tune.
    }

    void gl_context_eagl::pause() {
        if (m_paused || !m_context) {
            return;
        }
        if ([EAGLContext currentContext] == m_context) {
            glFinish();
            [EAGLContext setCurrentContext:nil];
        }
        m_paused = true;
    }

    void gl_context_eagl::resume() {
        if (!m_paused) {
            return;
        }
        m_paused = false;
        make_current();
    }

    void gl_context_eagl::update_surface(void *new_surface) {
        CAEAGLLayer *layer = (__bridge CAEAGLLayer *)new_surface;
        if (layer == m_layer) {
            return;
        }
        release_renderbuffers();
        attach_layer(layer);
    }

    bool gl_context_eagl::attach_layer(CAEAGLLayer *layer) {
        if (!m_context || !layer) {
            return false;
        }

        // CAEAGLLayer property mutations (opaque / drawableProperties) and the
        // -[EAGLContext renderbufferStorage:fromDrawable:] call (which sets
        // CALayer.contents internally) MUST happen on the main thread, or
        // UIKit raises an exception and the renderbuffer never gets a backing
        // store -- the EmulatorView then stays at clear color. The graphics
        // driver constructor runs on a worker thread, so bounce the layer-
        // touching work to the main queue via dispatch_sync. Pure GL state
        // (depth/stencil renderbuffer, FBO attachments) stays on the caller
        // thread; named GL objects are shared regardless of which thread
        // currently holds the EAGLContext.
        m_layer = layer;

        __block bool layer_ok = true;
        EAGLContext *ctx = m_context;
        CAEAGLLayer *target_layer = m_layer;
        __block GLuint colorbuffer = m_colorbuffer;
        __block GLint backing_width = 0;
        __block GLint backing_height = 0;

        dispatch_block_t main_work = ^{
            target_layer.opaque = YES;
            target_layer.drawableProperties = @{
                kEAGLDrawablePropertyRetainedBacking: @NO,
                kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8,
            };

            // EAGLContext is a per-thread "current" pointer. Borrow it for the
            // duration of the main-thread block so renderbufferStorage: binds
            // the storage to *our* colorbuffer; restore nil on the way out so
            // the worker thread can re-make_current and own it afterwards.
            EAGLContext *prev = [EAGLContext currentContext];
            [EAGLContext setCurrentContext:ctx];

            if (colorbuffer == 0) {
                glGenRenderbuffers(1, &colorbuffer);
            }
            glBindRenderbuffer(GL_RENDERBUFFER, colorbuffer);

            if (![ctx renderbufferStorage:GL_RENDERBUFFER fromDrawable:target_layer]) {
                LOG_ERROR(DRIVER_GRAPHICS, "EAGL renderbufferStorage:fromDrawable: failed");
                [EAGLContext setCurrentContext:prev];
                layer_ok = false;
                return;
            }

            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backing_width);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backing_height);

            [EAGLContext setCurrentContext:prev];
        };

        if ([NSThread isMainThread]) {
            main_work();
        } else {
            dispatch_sync(dispatch_get_main_queue(), main_work);
        }

        if (!layer_ok) {
            return false;
        }

        m_colorbuffer = colorbuffer;
        m_backbuffer_width = static_cast<std::uint32_t>(backing_width);
        m_backbuffer_height = static_cast<std::uint32_t>(backing_height);

        // Re-acquire the context on the caller thread for the remaining GL
        // setup and any draw work that follows.
        [EAGLContext setCurrentContext:m_context];

        if (m_framebuffer == 0) {
            glGenFramebuffers(1, &m_framebuffer);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_RENDERBUFFER, m_colorbuffer);

        if (m_depthbuffer == 0) {
            glGenRenderbuffers(1, &m_depthbuffer);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, backing_width, backing_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, m_depthbuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, m_depthbuffer);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR(DRIVER_GRAPHICS, "EAGL framebuffer incomplete (0x{:X})", status);
            return false;
        }
        return true;
    }

    void gl_context_eagl::release_renderbuffers() {
        if (m_colorbuffer != 0) {
            glDeleteRenderbuffers(1, &m_colorbuffer);
            m_colorbuffer = 0;
        }
        if (m_depthbuffer != 0) {
            glDeleteRenderbuffers(1, &m_depthbuffer);
            m_depthbuffer = 0;
        }
    }
}
