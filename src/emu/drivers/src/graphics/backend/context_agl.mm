// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "context_agl.h"
#include <common/log.h>

namespace eka2l1::drivers::graphics {
    static bool update_cached_dimensions(NSView* view, std::uint32_t* width, std::uint32_t* height) {
      NSWindow* window = [view window];
      NSSize size = [view frame].size;

      const CGFloat scale = [window backingScaleFactor];
      std::uint32_t new_width = static_cast<std::uint32_t>(size.width * scale);
      std::uint32_t new_height = static_cast<std::uint32_t>(size.height * scale);

      if (*width == new_width && *height == new_height)
        return false;

      *width = new_width;
      *height = new_height;
      return true;
    }

    static bool attach_context_to_view(NSOpenGLContext* context, NSView* view, std::uint32_t* width, std::uint32_t* height) {
      // Enable high-resolution display support.
      [view setWantsBestResolutionOpenGLSurface:YES];

      NSWindow* window = [view window];
      if (window == nil) {
        LOG_ERROR(DRIVER_GRAPHICS, "failed to get NSWindow");
        return false;
      }

      (void)update_cached_dimensions(view, width, height);

      // the following calls can crash if not called from the main thread on macOS 10.15
      dispatch_sync(dispatch_get_main_queue(), ^{
        [window makeFirstResponder:view];
        [context setView:view];
        [window makeKeyAndOrderFront:nil];
      });
      return true;
    }

    gl_context_agl::gl_context_agl(const window_system_info& wsi, bool stereo, bool core) {
      NSOpenGLPixelFormatAttribute attr[] = {
          NSOpenGLPFADoubleBuffer,
          NSOpenGLPFAOpenGLProfile,
          core ? NSOpenGLProfileVersion3_2Core : NSOpenGLProfileVersionLegacy,
          NSOpenGLPFAAccelerated,
          stereo ? NSOpenGLPFAStereo : static_cast<NSOpenGLPixelFormatAttribute>(0),
          0};
      m_pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
      if (m_pixel_format == nil) {
        LOG_ERROR(DRIVER_GRAPHICS, "failed to create pixel format");
        return;
      }

      m_context = [[NSOpenGLContext alloc] initWithFormat:m_pixel_format shareContext:nil];
      if (m_context == nil) {
        LOG_ERROR(DRIVER_GRAPHICS, "failed to create context");
        return;
      }

      if (!wsi.render_surface) {
        LOG_ERROR(DRIVER_GRAPHICS, "Render surface is not present!");
        return;
      }

      m_view = static_cast<NSView*>(wsi.render_surface);
      m_opengl_mode = mode::opengl;
      if (!attach_context_to_view(m_context, m_view, &m_backbuffer_width, &m_backbuffer_height))
        return;

      [m_context makeCurrentContext];
    }

    gl_context_agl::~gl_context_agl() {
      if ([NSOpenGLContext currentContext] == m_context)
        [NSOpenGLContext clearCurrentContext];

      if (m_context)
      {
        [m_context clearDrawable];
        [m_context release];
      }
      if (m_pixel_format)
        [m_pixel_format release];
    }

    bool gl_context_agl::is_headless() const {
      return !m_view;
    }

    void gl_context_agl::swap_buffers() {
      [m_context flushBuffer];
    }

    std::unique_ptr<gl_context> gl_context_agl::create_shared_context() {
      NSOpenGLContext* new_agl_context = [[NSOpenGLContext alloc] initWithFormat:m_pixel_format
                                                                    shareContext:m_context];
      if (new_agl_context == nil) {
        LOG_ERROR(DRIVER_GRAPHICS, "failed to create shared context");
        return nullptr;
      }

      std::unique_ptr<gl_context_agl> new_context = std::make_unique<gl_context_agl>();
      new_context->m_context = new_agl_context;
      new_context->m_pixel_format = m_pixel_format;
      [new_context->m_pixel_format retain];
      new_context->m_is_shared = true;
      return new_context;
    }

    bool gl_context_agl::make_current() {
      [m_context makeCurrentContext];
      return true;
    }

    bool gl_context_agl::clear_current() {
      [NSOpenGLContext clearCurrentContext];
      return true;
    }

    void gl_context_agl::update() {
      if (!m_view)
        return;

      if (update_cached_dimensions(m_view, &m_backbuffer_width, &m_backbuffer_height))
        // the following calls can crash if not called from the main thread on macOS 10.15
        dispatch_sync(dispatch_get_main_queue(), ^{
          [m_context update];
        });
    }

    void gl_context_agl::set_swap_interval(const std::int32_t interval) {
      [m_context setValues:static_cast<const GLint*>(&interval) forParameter:NSOpenGLCPSwapInterval];
    }
}

