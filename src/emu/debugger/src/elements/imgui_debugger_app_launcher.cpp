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
 */

#include <debugger/imgui_debugger.h>
#include <debugger/imgui_consts.h>
#include <debugger/renderer/common.h>

#include <config/config.h>
#include <common/language.h>
#include <common/cvt.h>
#include <kernel/kernel.h>

#include <services/window/window.h>
#include <services/window/bitmap_cache.h>
#include <services/fbs/bitmap.h>
#include <services/fbs/fbs.h>
#include <services/applist/applist.h>
#include <utils/apacmd.h>
#include <system/epoc.h>

#include <fmt/format.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace eka2l1 {
    static std::uint32_t HOVERED_COLOUR_ABGR = 0x80E6E6AD;
    static const char *ICON_PLACEHOLDER_PATH = "resources//placeholder_appicon.png";
    
    void imgui_debugger::show_app_list_classical(ImGuiTextFilter &app_search_box) {
        std::vector<apa_app_registry> &registerations = alserv->get_registerations();
        const float uid_col_size = ImGui::CalcTextSize("00000000").x + 30.0f;

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, uid_col_size);

        ImGuiListClipper clipper;
        bool should_step = false;

        if (app_search_box.Filters.empty()) {
            clipper.Begin(static_cast<int>(registerations.size()), ImGui::GetTextLineHeight());
            should_step = true;
        }

        while (!should_step || clipper.Step()) {
            const int clip_start = (app_search_box.Filters.empty()) ? clipper.DisplayStart : 0;
            const int clip_end = (app_search_box.Filters.empty()) ? clipper.DisplayEnd : static_cast<int>(registerations.size());

            for (int i = clip_start; i < clip_end; i++) {
                std::string name = " ";

                if (registerations[i].mandatory_info.long_caption.get_length() != 0) {
                    name += common::ucs2_to_utf8(registerations[i].mandatory_info.long_caption.to_std_string(nullptr));
                } else {
                    name += common::ucs2_to_utf8(registerations[i].mandatory_info.short_caption.to_std_string(nullptr));
                }

                const std::string uid_name = common::to_string(registerations[i].mandatory_info.uid, std::hex);

                if (app_search_box.PassFilter(name.c_str())) {
                    ImGui::Text("%s", uid_name.c_str());
                    ImGui::NextColumn();

                    ImGui::Text("%s", name.c_str());
                    ImGui::NextColumn();

                    if (ImGui::IsItemClicked()) {
                        // Launch app!
                        should_show_app_launch = false;
                        should_still_focus_on_keyboard = true;

                        epoc::apa::command_line cmdline;
                        cmdline.launch_cmd_ = epoc::apa::command_create;

                        kernel_system *kern = alserv->get_kernel_object_owner();

                        kern->lock();
                        alserv->launch_app(registerations[i], cmdline, nullptr);
                        kern->unlock();
                    }
                }
            }

            if (!should_step) {
                break;
            }
        }

        ImGui::Columns(1);
    }

    struct app_icon_render_data {
        apa_app_masked_icon_bitmap icon_;
        epoc::bitmap_cache *cache_;
        drivers::graphics_driver *driver_;

        eka2l1::vec2 offset_;
        float target_height_;

        drivers::handle custom_tex_;
    };

    static void app_list_icon_bitmap_draw_callback(const ImDrawList *list, const ImDrawCmd *cmd) {
        drivers::graphics_command_callback_data *dat = reinterpret_cast<drivers::graphics_command_callback_data *>(cmd->UserCallbackData);

        app_icon_render_data *tt = reinterpret_cast<app_icon_render_data*>(dat->userdata_);

        drivers::handle icon_tex = 0;
        drivers::handle icon_mask_tex = 0;

        if (tt->custom_tex_) {
            icon_tex = tt->custom_tex_;
        } else {
            icon_tex = tt->cache_->add_or_get(tt->driver_, dat->builder_, tt->icon_.first);

            if (tt->icon_.second)
                icon_mask_tex = tt->cache_->add_or_get(tt->driver_, dat->builder_, tt->icon_.second);
        }
        
        eka2l1::rect dest_rect;
        dest_rect.top.x = tt->offset_.x;
        dest_rect.top.y = tt->offset_.y;

        dest_rect.size.x = static_cast<int>(tt->target_height_);
        dest_rect.size.y = static_cast<int>(tt->target_height_);

        eka2l1::rect source_rect;

        source_rect.top.x = 0;
        source_rect.top.y = 0;
        source_rect.size = eka2l1::vec2(0, 0);

        dat->builder_->draw_bitmap(icon_tex, icon_mask_tex, dest_rect, source_rect, eka2l1::vec2(0, 0),
            0.0f, drivers::bitmap_draw_flag_invert_mask | drivers::bitmap_draw_flag_no_flip);

        delete tt;
    }

    void imgui_debugger::show_app_list_with_icons(ImGuiTextFilter &search) {
        std::vector<apa_app_registry> &registerations = alserv->get_registerations();

        ImGuiListClipper clipper;

        bool should_step = false;

        static constexpr float ICON_SIZE = 64.0f;
        static constexpr float ICON_BORDER_SELC = 1.0f;
        static constexpr float ICON_PADDING = 5.0f;
        static constexpr float ICON_PADDING_X = 10.0f;

        const int yadv = static_cast<int>(ICON_SIZE + ICON_PADDING);

        if (search.Filters.empty()) {
            clipper.Begin(static_cast<int>(registerations.size()), static_cast<float>(yadv));
            should_step = true;
        }

        const float padding_builtin = ImGui::GetStyle().WindowPadding.y;

        eka2l1::vec2 offset_display;
        eka2l1::vec2 content_start;

        content_start.x = static_cast<int>(ImGui::GetWindowPos().x);
        content_start.y = static_cast<int>(ImGui::GetWindowPos().y);

        offset_display.x = static_cast<int>(ImGui::GetStyle().WindowPadding.x);
        offset_display.y = static_cast<int>(padding_builtin);

        while (!should_step || clipper.Step()) {
            const int clip_start = (search.Filters.empty()) ? clipper.DisplayStart : 0;
            const int clip_end = (search.Filters.empty()) ? clipper.DisplayEnd : static_cast<int>(registerations.size());

            float initial_scroll = ImGui::GetScrollY();
            float cursor_pos_y = ImGui::GetCursorPosY();

            float cursor_screen_pos_x = ImGui::GetCursorScreenPos().x;
            float cursor_screen_pos_y = ImGui::GetCursorScreenPos().y;

            const int put_back_pos = static_cast<int>(initial_scroll) % static_cast<int>(yadv);
            offset_display.y -= put_back_pos;

            int passed = 0;

            for (int i = clip_start; i < clip_end; i++) {
                std::string name = " ";

                if (registerations[i].mandatory_info.long_caption.get_length() != 0) {
                    name += common::ucs2_to_utf8(registerations[i].mandatory_info.long_caption.to_std_string(nullptr));
                } else {
                    name += common::ucs2_to_utf8(registerations[i].mandatory_info.short_caption.to_std_string(nullptr));
                }

                const std::string uid_name = common::to_string(registerations[i].mandatory_info.uid, std::hex);

                if (search.PassFilter(name.c_str())) {
                    bool was_hovered = false;

                    ImVec2 offset_text_start;
                    offset_text_start.x += static_cast<float>(offset_display.x) + ICON_SIZE + ICON_PADDING_X; 
                    offset_text_start.y = cursor_pos_y + padding_builtin + yadv * passed;

                    ImRect collision_rect;

                    // Move to absolute position
                    ImVec2 collision_start = offset_text_start;
                    collision_start.x = cursor_screen_pos_x + static_cast<float>(offset_display.x);
                    collision_start.y -= (cursor_pos_y - cursor_screen_pos_y);

                    collision_rect.Min = collision_start;
                    collision_rect.Min.x -= ICON_BORDER_SELC;
                    collision_rect.Min.y -= ICON_BORDER_SELC;

                    collision_rect.Max = collision_start;
                    collision_rect.Max.x += ImGui::GetWindowContentRegionWidth() - ICON_BORDER_SELC * 2;
                    collision_rect.Max.y += ICON_BORDER_SELC + ICON_SIZE;

                    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                        ImGui::IsMouseHoveringRect(collision_rect.Min, collision_rect.Max, true)) {
                        ImGui::GetWindowDrawList()->AddRectFilled(collision_rect.Min, collision_rect.Max,
                            HOVERED_COLOUR_ABGR, 1.0f);

                        was_hovered = true;
                    }

                    auto opt_icon = alserv->get_icon(registerations[i], 0);
                    app_icon_render_data *data = new app_icon_render_data;

                    if (opt_icon.has_value())
                        data->icon_ = opt_icon.value();

                    data->cache_ = winserv->get_bitmap_cache();
                    data->driver_ = sys->get_graphics_driver();
                    data->offset_ = offset_display + content_start;
                    data->target_height_ = ICON_SIZE;

                    bool should_draw_icon = false;

                    if (!opt_icon.has_value()) {
                        static constexpr drivers::handle INVALID_HH = 0xFFFFFFFFFFFFFFFF;

                        if (icon_placeholder_icon != INVALID_HH) {
                            if (!icon_placeholder_icon) {
                                icon_placeholder_icon = renderer::load_texture_from_file_standalone(data->driver_,
                                    ICON_PLACEHOLDER_PATH, true);

                                if (!icon_placeholder_icon) {
                                    icon_placeholder_icon = INVALID_HH;
                                    delete data;
                                } else {
                                    data->custom_tex_ = icon_placeholder_icon;
                                    should_draw_icon = true;
                                }
                            } else {
                                data->custom_tex_ = icon_placeholder_icon;
                                should_draw_icon = true;
                            }
                        } else {
                            delete data;
                        }
                    } else {
                        data->custom_tex_ = 0;
                        should_draw_icon = true;
                    }

                    if (should_draw_icon) {
                        ImGui::GetWindowDrawList()->AddCallback(app_list_icon_bitmap_draw_callback, data);
                    }

                    const float height_of_text = ImGui::CalcTextSize(name.c_str()).y + ImGui::CalcTextSize(" UID: ").y;
                    offset_text_start.y += (ICON_SIZE - height_of_text - ImGui::GetStyle().ItemSpacing.y) / 2;

                    ImGui::SetCursorPos(offset_text_start);
                    ImGui::Text("%s", name.c_str());

                    ImGui::SetCursorPosX(offset_text_start.x);
                    ImGui::Text(" UID: 0x%08X", registerations[i].mandatory_info.uid);

                    if (was_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        // Launch app!
                        should_show_app_launch = false;
                        should_still_focus_on_keyboard = true;

                        epoc::apa::command_line cmdline;
                        cmdline.launch_cmd_ = epoc::apa::command_create;

                        kernel_system *kern = alserv->get_kernel_object_owner();

                        kern->lock();
                        alserv->launch_app(registerations[i], cmdline, nullptr);
                        kern->unlock();
                    }
                    
                    offset_display.y += yadv;
                    passed++;
                }
            }

            if (!should_step) {
                break;
            }
        }
    }

    void imgui_debugger::show_app_launch() {
        static ImGuiTextFilter app_search_box;

        const std::string app_launcher_title = common::get_localised_string(localised_strings, "app_launcher_title");
        ImGui::Begin(app_launcher_title.c_str(), &should_show_app_launch);

        if (alserv) {
            const float uid_col_size = ImGui::CalcTextSize("00000000").x + 30.0f;

            {
                const std::string search_txt = common::get_localised_string(localised_strings, "search");

                ImGui::Text("%s ", search_txt.c_str());
                ImGui::SameLine();

                app_search_box.Draw("##AppSearchBox");

                if (should_still_focus_on_keyboard) {
                    ImGui::SetKeyboardFocusHere(-1);
                    should_still_focus_on_keyboard = false;
                }

                // Allow new design on old architecture app list
                if (alserv->legacy_level() < APA_LEGACY_LEVEL_MORDEN) {
                    const std::string switch_title = common::get_localised_string(localised_strings, 
                        "app_launcher_switch_to_style_title");

                    const std::string switch_style_name = common::get_localised_string(localised_strings,
                        should_app_launch_use_new_style ?   "app_launcher_style_classical_str" :
                                                            "app_launcher_style_modern_str");

                    const std::string final_switch_str = switch_title + " " + switch_style_name;

                    const float switch_str_len = ImGui::CalcTextSize(final_switch_str.c_str()).x;
                    const float switch_str_len_button_size = static_cast<float>(switch_str_len) + 10.0f;

                    ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - switch_str_len_button_size);
                    if (ImGui::Button(final_switch_str.c_str(), ImVec2(switch_str_len_button_size, 0))) {
                        should_app_launch_use_new_style = !should_app_launch_use_new_style;

                        conf->ui_new_style = should_app_launch_use_new_style;
                        conf->serialize();
                    }
                } else {
                    should_app_launch_use_new_style = false;
                }

                if (!should_app_launch_use_new_style) {
                    ImGui::Columns(2);

                    ImGui::SetColumnWidth(0, uid_col_size + 8.0f);

                    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, " UID");
                    ImGui::NextColumn();

                    std::string name_str = " ";
                    name_str += common::get_localised_string(localised_strings, "name");

                    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", name_str.c_str());
                    ImGui::NextColumn();

                    ImGui::Separator();
                    ImGui::Columns(1);
                }
            }

            ImGui::BeginChild("##AppListScroll");
            {
                if (should_app_launch_use_new_style)
                    show_app_list_with_icons(app_search_box);
                else
                    show_app_list_classical(app_search_box);
                
                ImGui::EndChild();
            }
        } else {
            const std::string unavail_msg = common::get_localised_string(localised_strings, "app_launcher_unavail_msg");
            ImGui::Text("%s", unavail_msg.c_str());
        }

        ImGui::End();
    }
}