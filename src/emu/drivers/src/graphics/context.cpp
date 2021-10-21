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

#include <drivers/graphics/context.h>
#include <common/platform.h>

#if EKA2L1_PLATFORM(WIN32)
#include "backend/context_wgl.h"
#elif EKA2L1_PLATFORM(MACOS)
#include "backend/context_agl.h"
#elif EKA2L1_PLATFORM(UNIX)
#include "backend/context_glx.h"
#endif

namespace eka2l1::drivers::graphics {
    const std::array<std::pair<int, int>, 9> gl_context::s_desktop_opengl_versions = {
        {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2}}};

    std::unique_ptr<gl_context> make_gl_context(const drivers::window_system_info &system_info, const bool stereo, const bool core) {
#if EKA2L1_PLATFORM(WIN32)
        return std::make_unique<gl_context_wgl>(system_info, stereo, core);
#elif EKA2L1_PLATFORM(MACOS)
        return std::make_unique<gl_context_agl>(system_info, stereo, core);
#elif EKA2L1_PLATFORM(UNIX)
        return std::make_unique<gl_context_glx>(system_info, stereo, core);
#else
        return nullptr;
#endif
    }
}
