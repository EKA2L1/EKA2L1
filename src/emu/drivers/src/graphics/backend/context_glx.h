// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <GL/glx.h>
#include <GL/glxext.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <memory>
#include <thread>


#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <drivers/graphics/context.h>
//#include "Common/GL/GLX11Window.h"

namespace eka2l1::drivers::graphics {
    class gl_x11_render_window {
    public:
        explicit gl_x11_render_window(Display* display, Window parent_window, Colormap color_map, Window window, int width,
                  int height);
        ~gl_x11_render_window();

        Display* get_display() const { return m_display; }
        Window get_parent_window() const { return m_parent_window; }
        Window get_window() const { return m_window; }
        int get_width() const { return m_width; }
        int get_height() const { return m_height; }

        void update_dimensions();

        static std::unique_ptr<gl_x11_render_window> create(Display* display, Window parent_window,
             XVisualInfo* vi);

    private:
        Display* m_display;
        Window m_parent_window;
        Colormap m_color_map;
        Window m_window;

        int m_width;
        int m_height;
    };

    class gl_context_glx final : public gl_context {
    public:
        explicit gl_context_glx(const window_system_info& wsi, bool stereo, bool core);
        explicit gl_context_glx() = default;

        ~gl_context_glx() override;

        bool is_headless() const override;

        std::unique_ptr<gl_context> create_shared_context() override;

        bool make_current() override;
        bool clear_current() override;

        void update() override;

        void set_swap_interval(const std::int32_t interval) override;
        void swap_buffers() override;

    protected:
        Display* m_display = nullptr;
        std::unique_ptr<gl_x11_render_window> m_render_window;

        GLXDrawable m_drawable = {};
        GLXContext m_context = nullptr;
        GLXFBConfig m_fbconfig = {};
        bool m_supports_pbuffer = false;
        GLXPbufferSGIX m_pbuffer = 0;
        std::vector<int> m_attribs;

        bool create_window_surface(Window window_handle);
        void destroy_window_surface();
    };
}
