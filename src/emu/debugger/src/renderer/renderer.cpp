/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <common/vecx.h>
#include <debugger/renderer/renderer.h>
#include <drivers/graphics/imgui_renderer.h>
#include <drivers/graphics/graphics.h>
#include <drivers/input/input.h>

#include <imgui.h>

namespace eka2l1 {
    void debugger_renderer::init(drivers::graphic_api gr_api, drivers::graphics_driver_ptr graphic_driver, 
        drivers::input_driver_ptr input_driver, debugger_ptr dbg) {
        debugger = dbg;

        gr_driver_ = std::move(graphic_driver);
        inp_driver_ = std::move(input_driver);

        irenderer = drivers::make_imgui_renderer(gr_api);
        irenderer->init();
    }

    void debugger_renderer::draw(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width, std::uint32_t fb_height) {
        auto &io = ImGui::GetIO();

        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        io.DisplayFramebufferScale = ImVec2(
            width > 0 ? ((float)fb_width / width) : 0, height > 0 ? ((float)fb_height / height) : 0);

        ImGui::NewFrame();

        // Draw the imgui ui
        debugger->show_debugger(width, height, fb_width, fb_height);

        gr_driver_->process_requests();
        inp_driver_->process_requests();

        eka2l1::vec2 v = gr_driver_->get_screen_size();
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(v.x), static_cast<float>(v.y)));

        ImGui::Begin("Emulating Window", nullptr, ImGuiWindowFlags_NoResize);
        ImVec2 pos = ImGui::GetCursorScreenPos();

        if (ImGui::IsWindowFocused()) {
            inp_driver_->set_active(true);
        } else {
            inp_driver_->set_active(false);
        }

        //pass the texture of the FBO
        //window.getRenderTexture() is the texture of the FBO
        //the next parameter is the upper left corner for the uvs to be applied at
        //the third parameter is the lower right corner
        //the last two parameters are the UVs
        //they have to be flipped (normally they would be (0,0);(1,1)
        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(gr_driver_->get_render_texture_handle()),
            ImVec2(ImGui::GetCursorScreenPos()),
            ImVec2(ImGui::GetCursorScreenPos().x + gr_driver_->get_screen_size().x,
                ImGui::GetCursorScreenPos().y + gr_driver_->get_screen_size().y),
            ImVec2(0, 1), ImVec2(1, 0));

        //we are done working with this window
        ImGui::End();
        ImGui::EndFrame();

        ImGui::Render();

        irenderer->render(ImGui::GetDrawData());
    }

    void debugger_renderer::deinit() {
        irenderer->deinit();
    }
}
