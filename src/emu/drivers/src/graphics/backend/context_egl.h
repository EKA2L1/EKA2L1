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

#pragma once

#include <drivers/graphics/context.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace eka2l1::drivers::graphics {
    class gl_context_egl : public gl_context {
    public:
        explicit gl_context_egl() = default;
        explicit gl_context_egl(const window_system_info& wsi, bool stereo, bool core, bool gles);

        ~gl_context_egl() override;
        bool is_headless() const override;

        std::unique_ptr<gl_context> create_shared_context() override;

        bool make_current() override;
        bool clear_current() override;

        virtual void update(const std::uint32_t new_width, const std::uint32_t new_height) override;
        void update_surface(void *new_surface) override;

        void swap_buffers() override;
        void set_swap_interval(const std::int32_t interval) override;

    protected:
        EGLConfig egl_config;
        EGLSurface egl_surface;
        EGLContext egl_context;
        EGLDisplay egl_display;

        void *render_window;
        window_system_info wsi;

        std::pair<int, int> context_version;

        void init_gl();
        void init_surface();
        bool init_gl_for_shared(gl_context_egl *parent);
        bool create_context(EGLContext shared = nullptr, std::pair<int, int> *target_version = nullptr);

        virtual void create_surface();
        virtual void destroy_surface();

        virtual void prepare_render_window() {}
    };
}
