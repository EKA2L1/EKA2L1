// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <sstream>

#include <common/log.h>
#include "context_glx.h"

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092

typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSPROC)(Display*, GLXFBConfig, GLXContext, Bool,
                                                     const int*);
typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display*, GLXDrawable, int);
typedef int (*PFNGLXSWAPINTERVALMESAPROC)(unsigned int);

namespace eka2l1::drivers::graphics {
    static PFNGLXCREATECONTEXTATTRIBSPROC glXCreateContextAttribs = nullptr;
    static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXTPtr = nullptr;
    static PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESAPtr = nullptr;

    static PFNGLXCREATEGLXPBUFFERSGIXPROC glXCreateGLXPbufferSGIX = nullptr;
    static PFNGLXDESTROYGLXPBUFFERSGIXPROC glXDestroyGLXPbufferSGIX = nullptr;

    static bool s_glxError;

    static int ctx_error_handler(Display* dpy, XErrorEvent* ev) {
      s_glxError = true;
      return 0;
    }

    gl_x11_render_window::gl_x11_render_window(Display* display, Window parent_window, Colormap color_map, Window window,
        int width, int height)
        : m_display(display)
        , m_parent_window(parent_window)
        , m_color_map(color_map)
        , m_window(window)
        , m_width(width)
        , m_height(height) {
    }

    gl_x11_render_window::~gl_x11_render_window() {
        XUnmapWindow(m_display, m_window);
        XDestroyWindow(m_display, m_window);
        XFreeColormap(m_display, m_color_map);
    }

    void gl_x11_render_window::update_dimensions() {
        XWindowAttributes attribs;
        XGetWindowAttributes(m_display, m_parent_window, &attribs);
        XResizeWindow(m_display, m_window, attribs.width, attribs.height);
        m_width = attribs.width;
        m_height = attribs.height;
    }

    std::unique_ptr<gl_x11_render_window> gl_x11_render_window::create(Display* display, Window parent_window,
        XVisualInfo* vi) {
        // Set color map for the new window based on the visual.
        Colormap color_map = XCreateColormap(display, parent_window, vi->visual, AllocNone);
        XSetWindowAttributes attribs = {};
        attribs.colormap = color_map;

        // Get the dimensions from the parent window.
        XWindowAttributes parent_attribs = {};
        XGetWindowAttributes(display, parent_window, &parent_attribs);

        // Create the window
        Window window = XCreateWindow(display, parent_window, 0, 0, parent_attribs.width, parent_attribs.height, 0,
            vi->depth, InputOutput, vi->visual, CWColormap, &attribs);
        XSelectInput(display, parent_window, StructureNotifyMask);
        XMapWindow(display, window);
        XSync(display, True);

        return std::make_unique<gl_x11_render_window>(display, parent_window, color_map, window, parent_attribs.width, parent_attribs.height);
    }

    gl_context_glx::~gl_context_glx() {
      destroy_window_surface();
      if (m_context) {
        if (glXGetCurrentContext() == m_context)
          glXMakeCurrent(m_display, None, nullptr);

        glXDestroyContext(m_display, m_context);
      }
    }

    bool gl_context_glx::is_headless() const
    {
      return !m_render_window;
    }

    void gl_context_glx::set_swap_interval(const std::int32_t interval) {
        if (!m_drawable)
            return;

        // Try EXT_swap_control, then MESA_swap_control.
        if (glXSwapIntervalEXTPtr) {
            glXSwapIntervalEXTPtr(m_display, m_drawable, interval);
        } else if (glXSwapIntervalMESAPtr) {
            glXSwapIntervalMESAPtr(static_cast<unsigned int>(interval));
        } else {
            LOG_ERROR(DRIVER_GRAPHICS, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
        }
    }

    void gl_context_glx::swap_buffers() {
        glXSwapBuffers(m_display, m_drawable);
    }

    // Create rendering window.
    // Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
    gl_context_glx::gl_context_glx(const window_system_info& wsi, bool stereo, bool core) {
        m_display = static_cast<Display*>(wsi.display_connection);
        int screen = DefaultScreen(m_display);

        // checking glx version
        int glxMajorVersion, glxMinorVersion;
        glXQueryVersion(m_display, &glxMajorVersion, &glxMinorVersion);
        if (glxMajorVersion < 1 || (glxMajorVersion == 1 && glxMinorVersion < 4)) {
            LOG_ERROR(DRIVER_GRAPHICS, "glX-Version {}.{} detected, but need at least 1.4", glxMajorVersion,
                      glxMinorVersion);
            return;
        }

        // loading core context creation function
        glXCreateContextAttribs = (PFNGLXCREATECONTEXTATTRIBSPROC) glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB"));
        if (!glXCreateContextAttribs) {
            LOG_ERROR(DRIVER_GRAPHICS, "glXCreateContextAttribsARB not found, do you support GLX_ARB_create_context?");
            return;
        }

        // choosing framebuffer
        int visual_attribs[] = {GLX_X_RENDERABLE,
                      True,
                      GLX_DRAWABLE_TYPE,
                      GLX_WINDOW_BIT,
                      GLX_X_VISUAL_TYPE,
                      GLX_TRUE_COLOR,
                      GLX_RED_SIZE,
                      8,
                      GLX_GREEN_SIZE,
                      8,
                      GLX_BLUE_SIZE,
                      8,
                      GLX_DEPTH_SIZE,
                      0,
                      GLX_STENCIL_SIZE,
                      0,
                      GLX_DOUBLEBUFFER,
                      True,
                      GLX_STEREO,
                      stereo ? True : False,
                      None};
        int fbcount = 0;
        GLXFBConfig* fbc = glXChooseFBConfig(m_display, screen, visual_attribs, &fbcount);
        if (!fbc || !fbcount) {
            LOG_ERROR(DRIVER_GRAPHICS, "Failed to retrieve a framebuffer config");
            return;
        }
        m_fbconfig = *fbc;
        XFree(fbc);

        s_glxError = false;
        XErrorHandler oldHandler = XSetErrorHandler(&ctx_error_handler);

        // Create a GLX context.
        if (core) {
            for (const auto& version : s_desktop_opengl_versions) {
                std::array<int, 9> context_attribs = {
                    {GLX_CONTEXT_MAJOR_VERSION_ARB, version.first, GLX_CONTEXT_MINOR_VERSION_ARB,
                    version.second, GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                    GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, None}};

                s_glxError = false;
                m_context = glXCreateContextAttribs(m_display, m_fbconfig, 0, True, &context_attribs[0]);
                XSync(m_display, False);
                if (!m_context || s_glxError)
                    continue;

                // Got a context.
                LOG_ERROR(DRIVER_GRAPHICS, "Created a GLX context with version {}.{}", version.first, version.second);
                m_attribs.insert(m_attribs.end(), context_attribs.begin(), context_attribs.end());
                break;
            }
        }

        // Failed to create any core contexts, try for anything.
        if (!m_context || s_glxError) {
            std::array<int, 5> context_attribs_legacy = {
                {GLX_CONTEXT_MAJOR_VERSION_ARB, 1, GLX_CONTEXT_MINOR_VERSION_ARB, 0, None}};
            s_glxError = false;
            m_context = glXCreateContextAttribs(m_display, m_fbconfig, 0, True, &context_attribs_legacy[0]);
            XSync(m_display, False);
            m_attribs.clear();
            m_attribs.insert(m_attribs.end(), context_attribs_legacy.begin(), context_attribs_legacy.end());
        }

        if (!m_context || s_glxError) {
            LOG_ERROR(DRIVER_GRAPHICS, "Unable to create GL context.");
            XSetErrorHandler(oldHandler);
            return;
        }

        glXSwapIntervalEXTPtr = nullptr;
        glXSwapIntervalMESAPtr = nullptr;
        glXCreateGLXPbufferSGIX = nullptr;
        glXDestroyGLXPbufferSGIX = nullptr;
        m_supports_pbuffer = false;

        std::string tmp;
        std::istringstream buffer(glXQueryExtensionsString(m_display, screen));
        while (buffer >> tmp) {
            if (tmp == "GLX_SGIX_pbuffer") {
                glXCreateGLXPbufferSGIX = reinterpret_cast<PFNGLXCREATEGLXPBUFFERSGIXPROC>(
                    glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXCreateGLXPbufferSGIX")));
                glXDestroyGLXPbufferSGIX = reinterpret_cast<PFNGLXDESTROYGLXPBUFFERSGIXPROC>(
                    glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXDestroyGLXPbufferSGIX")));
                m_supports_pbuffer = glXCreateGLXPbufferSGIX && glXDestroyGLXPbufferSGIX;
            } else if (tmp == "GLX_EXT_swap_control") {
                glXSwapIntervalEXTPtr = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(
                    glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXSwapIntervalEXT")));
            } else if (tmp == "GLX_MESA_swap_control") {
                glXSwapIntervalMESAPtr = reinterpret_cast<PFNGLXSWAPINTERVALMESAPROC>(
                    glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXSwapIntervalMESA")));
            }
        }

        if (!create_window_surface(reinterpret_cast<Window>(wsi.render_surface))) {
            LOG_ERROR(DRIVER_GRAPHICS, "Error: CreateWindowSurface failed\n");
            XSetErrorHandler(oldHandler);
            return;
        }

        XSetErrorHandler(oldHandler);
        m_opengl_mode = mode::opengl;

        make_current();
    }

    std::unique_ptr<gl_context> gl_context_glx::create_shared_context() {
        s_glxError = false;
        XErrorHandler oldHandler = XSetErrorHandler(&ctx_error_handler);

        GLXContext new_glx_context = glXCreateContextAttribs(m_display, m_fbconfig, m_context, True, &m_attribs[0]);
        XSync(m_display, False);

        if (!new_glx_context || s_glxError) {
            LOG_ERROR(DRIVER_GRAPHICS, "Unable to create GL context.");
            XSetErrorHandler(oldHandler);
            return nullptr;
        }

        std::unique_ptr<gl_context_glx> new_context = std::make_unique<gl_context_glx>();
        new_context->m_context = new_glx_context;
        new_context->m_opengl_mode = m_opengl_mode;
        new_context->m_supports_pbuffer = m_supports_pbuffer;
        new_context->m_display = m_display;
        new_context->m_fbconfig = m_fbconfig;
        new_context->m_is_shared = true;

        if (m_supports_pbuffer && !new_context->create_window_surface(None)) {
            LOG_ERROR(DRIVER_GRAPHICS, "Error: CreateWindowSurface failed");
            XSetErrorHandler(oldHandler);
            return nullptr;
        }

        XSetErrorHandler(oldHandler);
        return new_context;
    }

    bool gl_context_glx::create_window_surface(Window window_handle) {
        if (window_handle) {
            // Get an appropriate visual
            XVisualInfo* vi = glXGetVisualFromFBConfig(m_display, m_fbconfig);
            m_render_window = gl_x11_render_window::create(m_display, window_handle, vi);
            if (!m_render_window)
            return false;

            m_backbuffer_width = m_render_window->get_width();
            m_backbuffer_height = m_render_window->get_height();
            m_drawable = static_cast<GLXDrawable>(m_render_window->get_window());
            XFree(vi);
        } else if (m_supports_pbuffer) {
            m_pbuffer = glXCreateGLXPbufferSGIX(m_display, m_fbconfig, 1, 1, nullptr);
            if (!m_pbuffer)
                return false;

            m_drawable = static_cast<GLXDrawable>(m_pbuffer);
        }

        return true;
    }

    void gl_context_glx::destroy_window_surface() {
        m_render_window.reset();
        if (m_supports_pbuffer && m_pbuffer) {
            glXDestroyGLXPbufferSGIX(m_display, m_pbuffer);
            m_pbuffer = 0;
        }
    }

    bool gl_context_glx::make_current() {
        return glXMakeCurrent(m_display, m_drawable, m_context);
    }

    bool gl_context_glx::clear_current() {
        return glXMakeCurrent(m_display, None, nullptr);
    }

    void gl_context_glx::update(const std::uint32_t new_width, const std::uint32_t new_height) {
        m_render_window->update_dimensions();
        m_backbuffer_width = m_render_window->get_width();
        m_backbuffer_height = m_render_window->get_height();
    }
}
