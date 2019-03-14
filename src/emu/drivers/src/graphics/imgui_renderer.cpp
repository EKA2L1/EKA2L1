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

#include <imgui.h>
#include <imgui_internal.h>

namespace eka2l1::drivers {
    void imgui_renderer_base::draw(drivers::handle h, const eka2l1::rect &rect) {
        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(h),
            ImVec2(static_cast<float>(rect.top.x), static_cast<float>(rect.top.y)),
            ImVec2(static_cast<float>(rect.top.x + rect.size.width()), 
                    static_cast<float>(rect.top.y + rect.size.height())),
            ImVec2(0, 1), ImVec2(1, 0));
    }
    
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
