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
#include <debugger/renderer/common.h>
#include <debugger/renderer/renderer.h>
#include <drivers/graphics/graphics.h>
#include <drivers/graphics/imgui_renderer.h>
#include <drivers/graphics/texture.h>

#include <imgui.h>
#include <manager/config.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <chrono>

namespace eka2l1 {
    void debugger_renderer::init(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
        debugger_base *debugger) {
        debugger_ = debugger;
        debugger_->renderer = this;
        background_change_path_.clear();

        irenderer_ = std::make_unique<drivers::imgui_renderer>();
        irenderer_->init(driver, builder);

        error_sheet.load(driver, builder, "resources//difficulties_sheet.png", "resources//difficulties_meta.xml", 7);

        if (!debugger_->get_config()->bkg_path.empty()) {
            change_background_internal(driver, builder, debugger_->get_config()->bkg_path.data());
        }
    }

    bool debugger_renderer::change_background(const char *path) {
        background_change_path_ = path;
        return true;
    }

    bool debugger_renderer::change_background_internal(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder, const char *path) {
        drivers::handle new_tex = renderer::load_texture_from_file(driver, builder, path);

        if (background_tex_ && new_tex) {
            // Free previous texture
            builder->destroy(background_tex_);
        }

        if (new_tex)
            background_tex_ = new_tex;

        return false;
    }

    void debugger_renderer::draw(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
        const std::uint32_t width, const std::uint32_t height, const std::uint32_t fb_width,
        const std::uint32_t fb_height) {
        const std::uint32_t scaled_width = static_cast<std::uint32_t>(width / debugger_->get_config()->ui_scale),
                            scaled_height = static_cast<std::uint32_t>(height / debugger_->get_config()->ui_scale);

        if (!background_change_path_.empty()) {
            // Update the background
            change_background_internal(driver, builder, background_change_path_.c_str());
            background_change_path_.clear();
        }

        auto &io = ImGui::GetIO();

        io.DisplaySize = ImVec2(static_cast<float>(scaled_width), static_cast<float>(scaled_height));
        io.DisplayFramebufferScale = ImVec2(
            scaled_width > 0 ? ((float)fb_width / scaled_width) : 0, scaled_height > 0 ? ((float)fb_height / scaled_height) : 0);

        ImGui::NewFrame();

        /*
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev_time).count();

        prev_time = now;

        error_sheet.play(static_cast<const float>(elapsed));
        ImVec2 uv_min, uv_max;

        error_sheet.get_current_frame_uv_coords(uv_min.x, uv_max.x, uv_min.y, uv_max.y);
        */

        // Draw the imgui ui
        debugger_->show_debugger(scaled_width, scaled_height, fb_width, fb_height);

        if (background_tex_) {
            manager::config_state *sstate = debugger_->get_config();

            ImGui::GetBackgroundDrawList()->AddImage(
                reinterpret_cast<ImTextureID>(background_tex_),
                ImVec2(0.0f, sstate->menu_height),
                ImVec2(static_cast<float>(scaled_width), static_cast<float>(scaled_height)),
                ImVec2(0, 0),
                ImVec2(1, 1),
                IM_COL32(255, 255, 255, sstate->bkg_transparency));
        }

        /*
        ImGui::GetBackgroundDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(error_sheet.sheet_),
            ImVec2(120.0f, 120.0f),
            ImVec2(120.0f + error_sheet.metas_[error_sheet.current_].size_.x, 120.0f + error_sheet.metas_[error_sheet.current_].size_.y),
            uv_min,
            uv_max
        );
        */

        ImGui::EndFrame();
        ImGui::Render();

        irenderer_->render(driver, builder, ImGui::GetDrawData());
    }

    void debugger_renderer::deinit(drivers::graphics_command_list_builder *builder) {
        irenderer_->deinit(builder);
    }
}
