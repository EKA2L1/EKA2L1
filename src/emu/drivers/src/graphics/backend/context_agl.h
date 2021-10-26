// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(__APPLE__) && defined(__OBJC__)
#import <AppKit/AppKit.h>
#else
struct NSOpenGLContext;
struct NSOpenGLPixelFormat;
struct NSView;
#endif

#include <drivers/graphics/context.h>

namespace eka2l1::drivers::graphics {
    class gl_context_agl final : public gl_context {
    public:
        explicit gl_context_agl() = default;
        explicit gl_context_agl(const window_system_info& wsi, bool stereo, bool core);

        ~gl_context_agl() override;
        bool is_headless() const override;

        std::unique_ptr<gl_context> create_shared_context() override;

        bool make_current() override;
        bool clear_current() override;

        void update() override;

        void swap_buffers() override;
        void set_swap_interval(const std::int32_t interval) override;

    protected:
        NSView* m_view = nullptr;
        NSOpenGLContext* m_context = nullptr;
        NSOpenGLPixelFormat* m_pixel_format = nullptr;
    };
}
