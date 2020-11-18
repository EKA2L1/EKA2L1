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

#include <common/language.h>
#include <common/cvt.h>
#include <config/config.h>
#include <drivers/itc.h>
#include <system/epoc.h>
#include <vfs/vfs.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace eka2l1 {
    void imgui_debugger::show_mount_sd_card() {
        if (!sd_card_mount_choosen) {
            if (!drivers::open_native_dialog(sys->get_graphics_driver(), "", [&](const char *result) {
                io_system *io = sys->get_io_system();
                
                io->unmount(drive_e);
                io->mount_physical_path(drive_e, drive_media::physical, io_attrib_removeable,
                    common::utf8_to_ucs2(result));

                sd_card_mount_choosen = true;
            }, true)) {
                should_show_sd_card_mount = false;
            }
        }

        if (sd_card_mount_choosen) {
            const std::string card_mount_succ_title = common::get_localised_string(localised_strings, "mmc_mount_success_dialog_title");
            const std::string card_mount_succ_msg = common::get_localised_string(localised_strings, "mmc_mount_success_dialog_msg");
            const std::string ok_str = common::get_localised_string(localised_strings, "ok");

            ImGui::OpenPopup(card_mount_succ_title.c_str());

            ImVec2 mount_success_window_size = ImVec2(400.0f, 150.0f);
            mount_success_window_size.x = ImGui::CalcTextSize(card_mount_succ_msg.c_str()).x + 20.0f;

            ImGui::SetNextWindowSize(mount_success_window_size);

            if (ImGui::BeginPopupModal(card_mount_succ_title.c_str())) {
                ImGui::Text("%s", card_mount_succ_msg.c_str());

                ImGui::NewLine();
                ImGui::NewLine();

                const std::string launch_app_str = common::get_localised_string(localised_strings, "file_menu_launch_apps_item_name");
                const float size_x_buttons = ImGui::CalcTextSize(launch_app_str.c_str()).x + 30.0f + 20.0f;

                ImGui::SameLine((mount_success_window_size.x - size_x_buttons) / 2);
                ImGui::PushItemWidth(30.0f);
                
                if (ImGui::Button(ok_str.c_str())) {
                    should_show_sd_card_mount = false;
                    sd_card_mount_choosen = false;
                }

                ImGui::PopItemWidth();
                ImGui::SameLine();

                if (ImGui::Button(launch_app_str.c_str())) {
                    should_show_sd_card_mount = false;
                    sd_card_mount_choosen = false;
                    should_show_app_launch = true;
                }

                ImGui::EndPopup();
            }
        }
    }

    void imgui_debugger::queue_error(const std::string &error) {
        const std::lock_guard<std::mutex> guard(errors_mut);
        error_queue.push(error);
    }

    void imgui_debugger::show_errors() {
        std::string first_error = "";

        {
            const std::lock_guard<std::mutex> guard(errors_mut);
            if (error_queue.empty()) {
                return;
            }

            first_error = error_queue.front();
        }

        const std::string error_title = common::get_localised_string(localised_strings, "error_dialog_title");
        ImGui::OpenPopup(error_title.c_str());

        if (ImGui::BeginPopupModal(error_title.c_str())) {
            ImGui::Text("%s", first_error.c_str());

            const std::string attached_msg = common::get_localised_string(localised_strings, "error_dialog_message");
            const std::string ok_str = common::get_localised_string(localised_strings, "ok");

            ImGui::Text(attached_msg.c_str());

            if (ImGui::Button(ok_str.c_str())) {
                const std::lock_guard<std::mutex> guard(errors_mut);
                error_queue.pop();
            }

            ImGui::EndPopup();
        }
    }

    void imgui_debugger::show_empty_device_warn() {
        ImGui::OpenPopup("##NoDevicePresent");
        if (ImGui::BeginPopupModal("##NoDevicePresent", &should_show_empty_device_warn)) {
            const std::string no_install_str = common::get_localised_string(localised_strings, "no_device_installed_msg");
            ImGui::Text("%s", no_install_str.c_str());

            std::string continue_button_title = common::get_localised_string(localised_strings, "continue");
            std::string install_device_title = common::get_localised_string(localised_strings, "no_device_installed_opt_install_device_btn_name");

            const float continue_button_title_width = ImGui::CalcTextSize(continue_button_title.c_str()).x;
            const float install_device_title_width = ImGui::CalcTextSize(install_device_title.c_str()).x;

            ImGui::NewLine();
            ImGui::SameLine((ImGui::GetWindowSize().x - continue_button_title_width - install_device_title_width - 10.0f) / 2);

            if (ImGui::Button(continue_button_title.c_str())) {
                should_show_empty_device_warn = false;
            }

            ImGui::SameLine();

            if (ImGui::Button(install_device_title.c_str())) {
                should_show_install_device_wizard = true;
                should_show_empty_device_warn = false;
            }

            ImGui::EndPopup();
        }
    }

    void imgui_debugger::show_touchscreen_disabled_warn() {
        ImGui::OpenPopup("##TouchscreenDisabledPop");
        if (ImGui::BeginPopupModal("##TouchscreenDisabledPop", nullptr)) {
            const std::string disabled_msg = common::get_localised_string(localised_strings, "touchscreen_disabled_msg");
            ImGui::Text("%s", disabled_msg.c_str());

            const std::string note_str = common::get_localised_string(localised_strings, "touchscreen_disabled_note_str");
            ImGui::TextColored(RED_COLOR, "%s: ", note_str.c_str());
            ImGui::SameLine();

            const std::string note_msg = common::get_localised_string(localised_strings, "touchscreen_disabled_note_msg");
            ImGui::Text("%s", note_msg.c_str());
            
            const std::string no_show = common::get_localised_string(localised_strings, "no_show_option_again_general_msg");
            ImGui::Checkbox(no_show.c_str(), &conf->stop_warn_touch_disabled);

            ImGui::NewLine();

            ImGui::SameLine((ImGui::GetWindowSize().x - 30.0f) / 2);
            ImGui::SetNextItemWidth(30.0f);

            if (ImGui::Button("OK")) {
                should_warn_touch_disabled = false;
                conf->serialize();
            }

            ImGui::EndPopup();
        }
    }
}