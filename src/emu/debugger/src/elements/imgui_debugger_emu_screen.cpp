/*
 * Copyright (c) 2018 EKA2L1 Team.
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
 * 
 * Copyright 2018-2020 Anton Shepelev (anton.txt@gmail.com)
 * 
 * Usage of the works is permitted  provided that this instrument is retained
 * with the works, so that any entity that uses the works is notified of this
 * instrument.    DISCLAIMER: THE WORKS ARE WITHOUT WARRANTY. 
 */

#include <debugger/imgui_debugger.h>
#include <debugger/renderer/renderer.h>

#include <config/config.h>
#include <system/epoc.h>

#include <services/window/window.h>
#include <services/window/screen.h>

#include <common/vecx.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace eka2l1 {
    static inline ImVec2 operator+(const ImVec2 &lhs, const ImVec2 &rhs) {
        return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
    }

    static inline ImVec2 operator-(const ImVec2 &lhs, const ImVec2 &rhs) {
        return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
    }

    // Taken from https://github.com/ocornut/imgui/issues/1982
    void imgui_image_rotate(ImTextureID tex_id, ImVec2 base_pos, ImVec2 center, ImVec2 size, float angle) {
        ImDrawList *draw_list = ImGui::GetWindowDrawList();

        angle = angle * static_cast<float>(common::PI) / 180.0f;

        float cos_a = cosf(angle);
        float sin_a = sinf(angle);

        center = ImVec2(-center.x, -center.y);

        ImVec2 pos[4] = {
            base_pos + ImRotate(ImVec2(center), cos_a, sin_a),
            base_pos + ImRotate(ImVec2(center.x + size.x, center.y), cos_a, sin_a),
            base_pos + ImRotate(ImVec2(center.x + size.x, center.y + size.y), cos_a, sin_a),
            base_pos + ImRotate(ImVec2(center.x, center.y + size.y), cos_a, sin_a)
        };

        ImVec2 uvs[4] = {
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            ImVec2(0.0f, 1.0f)
        };

        draw_list->AddImageQuad(tex_id, pos[0], pos[1], pos[2], pos[3], uvs[0], uvs[1], uvs[2], uvs[3], IM_COL32_WHITE);
    }

    static void draw_a_screen(ImTextureID id, ImVec2 base, ImVec2 size, const int rotation) {
        ImVec2 origin(0.0f, 0.0f);

        switch (rotation) {
        case 90:
            origin.y = size.y;
            break;

        case 180:
            origin = size;
            break;

        case 270:
            origin.x = size.x;
            break;

        default:
            break;
        }

        imgui_image_rotate(id, base, origin, size, static_cast<float>(rotation));
    }

    static void get_nice_scale(ImVec2 out_size, ImVec2 source_size, eka2l1::vec2 &scale, const bool integer_scale) {
        scale.x = scale.y = 1;

        if (!integer_scale) {
            while ((source_size.x * scale.x < out_size.x) && (source_size.y * scale.y < out_size.y)) {
                scale.x++;
                scale.y++;
            }

            if (scale.x > 1)
                scale.x--;
            if (scale.y > 1)
                scale.y--;

            return;
        }

        const double in_aspect_ratio = source_size.x / source_size.y;
        double aspect_normalized = 0.0;
        double exact_aspect_ratio = 0.0;

        if (in_aspect_ratio > 1.0)
            aspect_normalized = in_aspect_ratio;
        else
            aspect_normalized = 1.0 / in_aspect_ratio;

        exact_aspect_ratio = (aspect_normalized - floor(aspect_normalized)) < 0.01;

        std::int32_t current_scale_x = 0;
        std::int32_t current_scale_y = 0;
        std::int32_t max_scale_x = 0;
        std::int32_t max_scale_y = 0;

        max_scale_x = static_cast<std::int32_t>(floor(static_cast<double>(out_size.x / source_size.x)));
        max_scale_y = static_cast<std::int32_t>(floor(static_cast<double>(out_size.y / source_size.y)));

        current_scale_x = max_scale_x;
        current_scale_y = max_scale_y;

        double err_min = -1.0;

        double errpar = 0.0;
        double srat = 0.0;
        double errsize = 0.0;
        double err = 0.0;

        while(true) {
            const double parrat = static_cast<double>(current_scale_y) / current_scale_x / in_aspect_ratio;

            /* calculate aspect-ratio error: */
            if (parrat > 1.0 )
                errpar = parrat;
            else
                errpar = 1.0 / parrat;

            srat = common::min<double>(static_cast<double>(max_scale_y) / current_scale_y,
                static_cast<double>(max_scale_x) / current_scale_x);

            /* calculate size error: */
            /* if PAR is exact, exclude size error from the fitness function: */
            if (exact_aspect_ratio)
                errsize = 1.0;
            else
                errsize = std::pow(srat, 1.14f);

            err = errpar * errsize; /* total error */

            /* check for a new optimum: */
            if ((err < err_min) || (err_min == -1.0)) {
                scale.x = current_scale_x;
                scale.y = current_scale_y;

                err_min = err;
            }

            /* try a smaller magnification: */
            if (parrat < 1.0)
                current_scale_x--;
            else
                current_scale_y--;		

            /* do not explore magnifications smaller than half the screen: */
            if (srat >= 2.0)
                break;
        }
    }
    
    static void imgui_screen_aspect_resize_keeper(ImGuiSizeCallbackData* data) {
        float ratio = *reinterpret_cast<float*>(data->UserData);
        data->DesiredSize.y = data->DesiredSize.x / ratio + ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
    }

    void imgui_debugger::show_screens() {
        // Iterate through all screen from window server
        if (!winserv) {
            return;
        }

        epoc::screen *scr = winserv->get_screens();
        const bool fullscreen_now = renderer->is_fullscreen();
        ImGuiIO &io = ImGui::GetIO();

        for (std::uint32_t i = 0; scr && scr->screen_texture; i++, scr = scr->next) {
            if (fullscreen_now && scr->number != active_screen) {
                continue;
            }

            const std::string name = fmt::format(common::get_localised_string(localised_strings, "emulated_screen_title"),
                scr->number);
            eka2l1::vec2 size = scr->current_mode().size;

            scr->screen_mutex.lock();

            ImVec2 rotated_size(static_cast<float>(size.x), static_cast<float>(size.y));

            if (scr->ui_rotation % 180 == 90) {
                const float temp = rotated_size.x;

                rotated_size.x = rotated_size.y;
                rotated_size.y = temp;
            }

            ImVec2 fullscreen_region = ImVec2(io.DisplaySize.x, io.DisplaySize.y);

            const float fullscreen_start_x = 0.0f;
            const float fullscreen_start_y = 0.0f;

            float ratioed = rotated_size.x / rotated_size.y;

            if (fullscreen_now) {
                ImGui::SetNextWindowSize(fullscreen_region);
                ImGui::SetNextWindowPos(ImVec2(fullscreen_start_x, fullscreen_start_y));
            } else {
                ImGui::SetNextWindowSize(rotated_size, ImGuiCond_Once);

                if (back_from_fullscreen) {
                    ImGui::SetNextWindowPos(ImVec2(fullscreen_start_x, fullscreen_start_y + 30.0f));
                    ImGui::SetNextWindowSize(ImVec2(rotated_size.x * last_scale, rotated_size.y * last_scale));
                    
                    back_from_fullscreen = false;
                }

                ImGui::SetNextWindowSizeConstraints(rotated_size, fullscreen_region, imgui_screen_aspect_resize_keeper,
                    &ratioed);
            }
            
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

            if (fullscreen_now)
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGui::Begin(name.c_str(), nullptr, fullscreen_now ? (ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize) : 0);

            if (ImGui::IsWindowFocused()) {
                active_screen = scr->number;
            }

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RectOnly) && (conf->hide_mouse_in_screen_space && !io.KeyAlt)) {    
                ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            }

            ImVec2 winpos = ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));
            if (fullscreen_now ? ImGui::BeginPopupContextWindow(nullptr, ImGuiMouseButton_Right) : ImGui::BeginPopupContextItem(nullptr, ImGuiMouseButton_Right)) {
                bool orientation_lock = scr->orientation_lock;

                const std::string orientation_lock_str = common::get_localised_string(localised_strings, "emulated_screen_orientation_lock_item_name");
                if (ImGui::BeginMenu(orientation_lock_str.c_str(), false)) {
                    orientation_lock = !orientation_lock;
                    ImGui::EndMenu();
                }

                scr->set_orientation_lock(sys->get_graphics_driver(), orientation_lock);

                const std::string rotation_str = common::get_localised_string(localised_strings, "emulated_screen_rotation_item_name");
                if (ImGui::BeginMenu(rotation_str.c_str())) {
                    bool selected[4] = { (scr->ui_rotation == 0),
                        (scr->ui_rotation == 90),
                        (scr->ui_rotation == 180),
                        (scr->ui_rotation == 270) };

                    ImGui::MenuItem("0", nullptr, &selected[0]);
                    ImGui::MenuItem("90", nullptr, &selected[1]);
                    ImGui::MenuItem("180", nullptr, &selected[2]);
                    ImGui::MenuItem("270", nullptr, &selected[3]);

                    for (std::uint32_t i = 0; i <= 3; i++) {
                        if (selected[i] && (i != scr->ui_rotation / 90)) {
                            scr->set_rotation(sys->get_graphics_driver(), i * 90);
                            break;
                        }
                    }

                    ImGui::EndMenu();
                }

                const std::string refresh_str = common::get_localised_string(localised_strings, "emulated_screen_refresh_rate_item_name");
                if (ImGui::BeginMenu(refresh_str.c_str())) {
                    int current_refresh_rate = scr->refresh_rate;
                    ImGui::SliderInt("##RefreshRate", &current_refresh_rate, 0, 120);

                    if (current_refresh_rate != scr->refresh_rate) {
                        scr->refresh_rate = static_cast<std::uint8_t>(current_refresh_rate);
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndPopup();
            }
            
            ImGui::PopStyleVar();

            eka2l1::vec2 scale(0, 0);

            if (fullscreen_now) {
                get_nice_scale(fullscreen_region, ImVec2(static_cast<float>(rotated_size.x),
                    static_cast<float>(rotated_size.y)), scale, conf->integer_scaling);
                
                scr->scale_x = static_cast<float>(scale.x);
                scr->scale_y = static_cast<float>(scale.y);
            } else {
                scr->scale_x = ImGui::GetWindowSize().x / rotated_size.x;
                scr->scale_y = scr->scale_x;

                last_scale = scr->scale_x;
            }

            ImVec2 scaled_no_dsa;
            ImVec2 scaled_dsa;

            if (scr->ui_rotation % 180 != 0) {
                std::swap(scr->scale_x, scr->scale_y);
            }

            const eka2l1::vec2 size_dsa_org = scr->size();

            // Different dynamic UI rotation, must reverse for DSA
            if ((scr->ui_rotation % 180) != (scr->current_mode().rotation % 180)) {
                scaled_dsa.x = static_cast<float>(size_dsa_org.x) * scr->scale_y;
                scaled_dsa.y = static_cast<float>(size_dsa_org.y) * scr->scale_x;
            } else {
                scaled_dsa.x = static_cast<float>(size_dsa_org.x) * scr->scale_x;
                scaled_dsa.y = static_cast<float>(size_dsa_org.y) * scr->scale_y;
            }

            if (fullscreen_now) {
                const eka2l1::vec2 org_screen_size = scr->size();

                scaled_no_dsa.x = static_cast<float>(size.x * scale.x);
                scaled_no_dsa.y = static_cast<float>(size.y * scale.y);

                if (scr->ui_rotation % 180 != 0) {
                    winpos.x += (fullscreen_region.x - scaled_no_dsa.y) / 2;
                    winpos.y += (fullscreen_region.y - scaled_no_dsa.x) / 2;
                } else {
                    winpos.x += (fullscreen_region.x - scaled_no_dsa.x) / 2;
                    winpos.y += (fullscreen_region.y - scaled_no_dsa.y) / 2;
                }
            } else {
                scaled_no_dsa = ImGui::GetWindowSize();
                scaled_no_dsa.y -= ImGui::GetCurrentWindow()->TitleBarHeight();
            }

            if ((scr->ui_rotation % 180) != 0) {
                std::swap(scaled_no_dsa.x, scaled_no_dsa.y);
            }

            scr->absolute_pos.x = static_cast<int>(winpos.x);
            scr->absolute_pos.y = static_cast<int>(winpos.y);

            draw_a_screen(reinterpret_cast<ImTextureID>(scr->screen_texture), winpos, scaled_no_dsa, scr->ui_rotation);

            if (scr->dsa_texture) {
                const int rotation = (scr->current_mode().rotation + scr->ui_rotation) % 360;

                draw_a_screen(reinterpret_cast<ImTextureID>(scr->dsa_texture), winpos, scaled_dsa, rotation);
            }

            scr->screen_mutex.unlock();

            ImGui::End();

            if (fullscreen_now) {
                ImGui::PopStyleVar();
            }

            ImGui::PopStyleVar();

            if (fullscreen_now) {
                break;
            }
        }
    }
}