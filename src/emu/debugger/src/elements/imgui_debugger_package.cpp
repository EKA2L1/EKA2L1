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
#include <package/manager.h>

#include <drivers/itc.h>
#include <system/epoc.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <common/algorithm.h>
#include <common/language.h>
#include <common/cvt.h>

namespace eka2l1 {
    void imgui_debugger::do_install_package() {
        std::string path = "";

        on_pause_toogle(true);

        drivers::open_native_dialog(
            sys->get_graphics_driver(), "sis,sisx", [&](const char *res) {
                path = res;
            },
            false);

        should_pause = false;
        on_pause_toogle(false);

        if (!path.empty()) {
            install_list.push(path);
            install_thread_cond.notify_one();
        }
    }

    void imgui_debugger::show_package_manager() {
        // Get package manager
        manager::packages *manager = sys->get_packages();

        const std::string pack_mngr_title = common::get_localised_string(localised_strings, "packages_title");
        ImGui::Begin(pack_mngr_title.c_str(), &should_package_manager, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            const std::string package_menu_name = common::get_localised_string(localised_strings, "packages_package_menu_name");

            if (ImGui::BeginMenu(package_menu_name.c_str())) {
                const std::string install_menu_name = common::get_localised_string(localised_strings, "packages_install_string");
                const std::string remove_menu_name = common::get_localised_string(localised_strings, "packages_remove_string");
                const std::string filelist_menu_name = common::get_localised_string(localised_strings, "packages_file_lists_string");
                
                if (ImGui::MenuItem(install_menu_name.c_str())) {
                    do_install_package();
                }

                if (ImGui::MenuItem(remove_menu_name.c_str(), nullptr, &should_package_manager_remove)) {
                    should_package_manager_remove = true;
                }

                if (ImGui::MenuItem(filelist_menu_name.c_str(), nullptr, &should_package_manager_display_file_list)) {
                    should_package_manager_display_file_list = true;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
        ImGui::Columns(4);

        ImGui::SetColumnWidth(0, ImGui::CalcTextSize("0x11111111").x + 20.0f);

        const std::string name_str = common::get_localised_string(localised_strings, "name");
        const std::string vendor_str = common::get_localised_string(localised_strings, "packages_package_vendor_string");
        const std::string drive_str = common::get_localised_string(localised_strings, "packages_package_drive_located_string");

        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "UID");
        ImGui::NextColumn();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", name_str.c_str());
        ImGui::NextColumn();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", vendor_str.c_str());
        ImGui::NextColumn();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", drive_str.c_str());
        ImGui::NextColumn();

        ImGuiListClipper clipper(static_cast<int>(manager->package_count()), ImGui::GetTextLineHeight());

        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const manager::package_info *pkg = manager->package(i);
                std::string str = "0x" + common::to_string(pkg->id, std::hex);

                ImGui::Text("%s", str.c_str());
                ImGui::NextColumn();

                str = common::ucs2_to_utf8(pkg->name);

                if (ImGui::Selectable(str.c_str(), selected_package_index == i)) {
                    if (!should_package_manager_remove) {
                        selected_package_index = i;
                        should_package_manager_display_file_list = false;
                    }
                }

                if (selected_package_index == i && ImGui::BeginPopupContextItem()) {
                    const std::string filelist_menu_name = common::get_localised_string(localised_strings, "packages_file_lists_string");
                    const std::string remove_menu_name = common::get_localised_string(localised_strings, "packages_remove_string");
                    
                    if (ImGui::MenuItem(filelist_menu_name.c_str())) {
                        should_package_manager_display_file_list = true;
                    }

                    if (ImGui::MenuItem(remove_menu_name.c_str())) {
                        should_package_manager_remove = true;
                    }

                    ImGui::EndPopup();
                }

                ImGui::NextColumn();

                str = common::ucs2_to_utf8(pkg->vendor_name);

                ImGui::Text("%s", str.c_str());
                ImGui::NextColumn();

                str = static_cast<char>(drive_to_char16(pkg->drive));
                str += ":";

                ImGui::Text("%s", str.c_str());
                ImGui::NextColumn();
            }
        }

        ImGui::Columns(1);
        ImGui::End();

        if (should_package_manager_display_file_list) {
            const std::lock_guard<std::mutex> guard(manager->lockdown);
            const manager::package_info *pkg = manager->package(selected_package_index);

            if (pkg != nullptr) {
                const std::string pkg_title = common::ucs2_to_utf8(pkg->name) + " (0x" + common::to_string(pkg->id, std::hex) + ") file lists";

                std::vector<std::string> paths;
                manager->get_file_bucket(pkg->id, paths);

                ImGui::Begin(pkg_title.c_str(), &should_package_manager_display_file_list);

                for (auto &path : paths) {
                    ImGui::Text("%s", path.c_str());
                }

                ImGui::End();
            }
        }

        if (should_package_manager_remove) {
            std::unique_lock<std::mutex> guard(manager->lockdown);
            const manager::package_info *pkg = manager->package(selected_package_index);

            const std::string info_str = common::get_localised_string(localised_strings, "info");
            ImGui::OpenPopup(info_str.c_str());

            if (ImGui::BeginPopupModal(info_str.c_str(), &should_package_manager_remove)) {
                if (pkg == nullptr) {
                    const std::string no_package_str = common::get_localised_string(localised_strings, "packages_no_package_select_msg");
                    ImGui::Text("%s", no_package_str.c_str());
                } else {
                    const std::string ask_question = fmt::format(common::get_localised_string(localised_strings,
                        "packages_remove_confirmation_msg"), common::ucs2_to_utf8(pkg->name));

                    ImGui::Text("%s", ask_question.c_str());
                }

                ImGui::NewLine();

                ImGui::SameLine(ImGui::CalcItemWidth() / 2 + 20.0f);

                const std::string yes_str = common::get_localised_string(localised_strings, "yes");
                const std::string no_str = common::get_localised_string(localised_strings, "no");
                    
                if (ImGui::Button(yes_str.c_str())) {
                    should_package_manager_remove = false;
                    should_package_manager_display_file_list = false;

                    if (pkg != nullptr) {
                        guard.unlock();
                        manager->uninstall_package(pkg->id);
                        guard.lock();
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button(no_str.c_str())) {
                    should_package_manager_remove = false;
                }

                ImGui::EndPopup();
            }
        }
    }

    void imgui_debugger::show_installer_text_popup() {
        const std::string installer_popup_title = common::get_localised_string(localised_strings, "installer_text_popup_title");

        ImGui::OpenPopup(installer_popup_title.c_str());
        ImGuiIO &io = ImGui::GetIO();

        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x / 4, io.DisplaySize.y / 3));

        if (ImGui::BeginPopupModal(installer_popup_title.c_str())) {
            ImGui::Text("%s", installer_text);
            ImGui::NewLine();

            const float button_size = 30.0f;
            float total_width_button_occupied = button_size;

            if (!should_package_manager_display_one_button_only) {
                total_width_button_occupied += button_size + 4.0f;
            }

            ImGui::SameLine((ImGui::GetWindowWidth() - total_width_button_occupied) / 2);

            const std::string yes_str = common::get_localised_string(localised_strings, "yes");
            const std::string no_str = common::get_localised_string(localised_strings, "no");

            if (ImGui::Button(yes_str.c_str())) {
                installer_text_result = true;
                installer_cond.notify_one();

                should_package_manager_display_installer_text = false;
            }

            ImGui::SameLine();

            if (!should_package_manager_display_one_button_only && ImGui::Button(no_str.c_str())) {
                installer_text_result = false;
                installer_cond.notify_one();

                should_package_manager_display_installer_text = false;
            }

            ImGui::EndPopup();
        }
    }

    void imgui_debugger::show_installer_choose_lang_popup() {
        const std::string choose_lang_str = common::get_localised_string(localised_strings, "installer_language_choose_title");
        ImGui::OpenPopup(choose_lang_str.c_str());

        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x / 4, io.DisplaySize.y / 3));

        if (ImGui::BeginPopupModal(choose_lang_str.c_str())) {
            for (int i = 0; i < installer_lang_size; i++) {
                if (ImGui::Selectable(num_to_lang(installer_langs[i]), installer_current_lang_idx == i)) {
                    installer_current_lang_idx = i;
                }
            }

            ImGui::NewLine();

            const float button_size = 30.0f;
            float total_width_button_occupied = button_size * 2 + 4.0f;

            ImGui::SameLine((ImGui::GetWindowWidth() - total_width_button_occupied) / 2);

            const std::string ok_str = common::get_localised_string(localised_strings, "ok");
            const std::string cancel_str = common::get_localised_string(localised_strings, "cancel");

            if (ImGui::Button(ok_str.c_str())) {
                installer_lang_choose_result = installer_langs[installer_current_lang_idx];
                installer_cond.notify_one();

                should_package_manager_display_language_choose = false;
            }

            ImGui::SameLine();

            if (ImGui::Button(cancel_str.c_str())) {
                installer_cond.notify_one();
                should_package_manager_display_language_choose = false;
            }

            ImGui::EndPopup();
        }
    }
}