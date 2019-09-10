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
#include <drivers/graphics/graphics.h>
#include <drivers/graphics/imgui_renderer.h>
#include <drivers/graphics/texture.h>

#include <manager/config.h>

#include <imgui.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace eka2l1 {
    void debugger_renderer::init(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
        debugger_base *debugger) {
        debugger_ = debugger;
        debugger_->renderer = this;
        background_change_path_.clear();

        irenderer_ = std::make_unique<drivers::imgui_renderer>();
        irenderer_->init(driver, builder);

        if (!debugger_->get_config()->bkg_path.empty()) {
            change_background_internal(driver, builder, debugger_->get_config()->bkg_path.data());
        }
    }

    bool debugger_renderer::change_background(const char *path) {
        background_change_path_ = path;
        return true;
    }

    bool debugger_renderer::change_background_internal(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder, const char *path) {
        if (!path || strlen(path) == 0) {
            return false;
        }

        int width = 0;
        int height = 0;
        int comp = 0;

        unsigned char *dat = stbi_load(path, &width, &height, &comp, STBI_rgb_alpha);

        if (dat == nullptr) {
            return false;
        }

        if (background_tex_) {
            // Free previous texture
            builder->destroy(background_tex_);
        }

        background_tex_ = drivers::create_texture(driver, 2, 0, drivers::texture_format::rgba, drivers::texture_format::rgba,
            drivers::texture_data_type::ubyte, dat, { width, height, 0 });
        builder->set_texture_filter(background_tex_, drivers::filter_option::linear, drivers::filter_option::linear);

        stbi_image_free(dat);
        return true;
    }

    void debugger_renderer::draw(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
        const std::uint32_t width, const std::uint32_t height, const std::uint32_t fb_width,
        const std::uint32_t fb_height) {
        if (!background_change_path_.empty()) {
            // Update the background
            change_background_internal(driver, builder, background_change_path_.c_str());
            background_change_path_.clear();
        }

        auto &io = ImGui::GetIO();

        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        io.DisplayFramebufferScale = ImVec2(
            width > 0 ? ((float)fb_width / width) : 0, height > 0 ? ((float)fb_height / height) : 0);

        ImGui::NewFrame();

        // Draw the imgui ui
        debugger_->show_debugger(width, height, fb_width, fb_height);

        if (background_tex_) {
            manager::config_state *sstate = debugger_->get_config();

            ImGui::GetBackgroundDrawList()->AddImage(
                reinterpret_cast<ImTextureID>(background_tex_),
                ImVec2(0.0f, sstate->menu_height),
                ImVec2(static_cast<float>(width), static_cast<float>(height)),
                ImVec2(0, 0),
                ImVec2(1, 1),
                IM_COL32(255, 255, 255, sstate->bkg_transparency));
        }

        /*
        eka2l1::vec2 v = gr_driver_->get_screen_size();
        auto padding = ImGui::GetStyle().WindowPadding;
        v = vec2(static_cast<const int>(v.x + ImGui::GetStyle().WindowPadding.x * 2),
            static_cast<const int>(v.y + (ImGui::GetStyle().WindowPadding.y + 10) * 2));

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
            
*/
        //we are done working with this window
        ImGui::End();
        ImGui::EndFrame();
        ImGui::Render();

        irenderer_->render(driver, builder, ImGui::GetDrawData());
    }

    void debugger_renderer::deinit(drivers::graphics_command_list_builder *builder) {
        irenderer_->deinit(builder);
    }
}
