/*
 * Copyright (c) 2021 EKA2L1 Team.
 * Copyright 2008 Dolphin Emulator Project.
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

#include <drivers/graphics/common.h>

#include <array>
#include <cstdint>
#include <memory>
#include <tuple>

namespace eka2l1::drivers::graphics {
    class gl_context {
    public:
        enum class mode {
            detect,
            opengl,
            opengl_es
        };

    protected:
        mode m_opengl_mode = mode::detect;

        // Window dimensions.
        std::uint32_t m_backbuffer_width = 0;
        std::uint32_t m_backbuffer_height = 0;

        bool m_is_shared = false;

        // A list of desktop OpenGL versions to attempt to create a context for (4.6-3.0).
        static const std::array<std::pair<int, int>, 11> s_desktop_opengl_versions;

    public:
        virtual ~gl_context() = default;

        virtual bool make_current() = 0;
        virtual bool clear_current() = 0;

        virtual void swap_buffers() = 0;
        virtual void update(const std::uint32_t new_width, const std::uint32_t new_height) = 0;
        virtual void set_swap_interval(const std::int32_t interval) = 0;

        virtual bool is_headless() const = 0;
        virtual void update_surface(void *new_surface) {}

        virtual std::unique_ptr<gl_context> create_shared_context() = 0;

        mode gl_mode() const {
            return m_opengl_mode;
        }
    };

    std::unique_ptr<gl_context> make_gl_context(const drivers::window_system_info &system_info, const bool stereo = false, const bool core = true);
}
