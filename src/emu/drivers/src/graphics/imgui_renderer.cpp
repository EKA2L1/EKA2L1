/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <drivers/graphics/imgui_renderer.h>
#include <drivers/graphics/backend/ogl/imgui_renderer_ogl.h>

namespace eka2l1::drivers {
    imgui_renderer_instance make_imgui_renderer(const graphic_api api) {
        switch (api) {
        case graphic_api::opengl: {
            return std::make_unique<ogl_imgui_renderer>();
            break;
        }

        default:
            break;
        }

        return nullptr;
    }
}
