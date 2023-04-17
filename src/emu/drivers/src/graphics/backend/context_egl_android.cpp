/*
 * Copyright (c) 2021 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
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

#include "context_egl_android.h"
#include <android/native_window.h>

#include <common/log.h>

namespace eka2l1::drivers::graphics {
    gl_context_egl_android::gl_context_egl_android(const window_system_info& wsi, bool stereo, bool core)
        : gl_context_egl(wsi, stereo, core, true) {
    }

    void gl_context_egl_android::create_surface() {
        if (!render_window) {
            return;
        }

        EGLint format;
        eglGetConfigAttrib(egl_display, egl_config, EGL_NATIVE_VISUAL_ID, &format);
        ANativeWindow_setBuffersGeometry(reinterpret_cast<ANativeWindow*>(render_window), 0, 0, format);

        gl_context_egl::create_surface();
    }
}