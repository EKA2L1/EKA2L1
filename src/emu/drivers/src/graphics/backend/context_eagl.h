// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
@class EAGLContext;
@class CAEAGLLayer;
#else
struct EAGLContext;
struct CAEAGLLayer;
#endif

#include <drivers/graphics/context.h>

namespace eka2l1::drivers::graphics {
    // EAGL-backed GLES3 context for the iOS frontend.
    //
    // Constructed with `window_system_info::render_surface` pointing at a
    // CAEAGLLayer (set up by the iOS frontend's EAGLView). The
    // class owns the EAGLContext, a framebuffer-object, and the colour /
    // depth-stencil renderbuffers bound to that layer; swap_buffers presents
    // the colour renderbuffer to the layer's drawable.
    class gl_context_eagl final : public gl_context {
    public:
        gl_context_eagl() = default;
        gl_context_eagl(const window_system_info &wsi, bool stereo, bool core);

        ~gl_context_eagl() override;

        bool is_headless() const override;

        std::unique_ptr<gl_context> create_shared_context() override;

        bool make_current() override;
        bool clear_current() override;

        void swap_buffers() override;
        void update(std::uint32_t new_width, std::uint32_t new_height) override;
        void set_swap_interval(std::int32_t interval) override;

        // The iOS lifecycle requires GL traffic to stop before the app loses
        // foreground. The frontend calls pause() from `scenePhase` ≠ .active
        // (glFinish + clear current) and resume() when it returns.
        void pause();
        void resume();

        // Replace the bound CAEAGLLayer (e.g. after the EAGLView is recreated
        // by SwiftUI on a navigation transition).
        void update_surface(void *new_surface) override;

        unsigned int swapchain_framebuffer() const override {
            return m_framebuffer;
        }

    private:
        bool attach_layer(CAEAGLLayer *layer);
        void release_renderbuffers();

        EAGLContext *m_context = nullptr;
        CAEAGLLayer *m_layer = nullptr;

        unsigned int m_framebuffer = 0;
        unsigned int m_colorbuffer = 0;
        unsigned int m_depthbuffer = 0;

        bool m_paused = false;
    };
}
