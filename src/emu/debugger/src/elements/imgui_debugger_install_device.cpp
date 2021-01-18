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

#include <common/fileutils.h>
#include <common/language.h>
#include <common/path.h>
#include <config/config.h>

#include <drivers/itc.h>
#include <system/devices.h>
#include <system/epoc.h>

#include <system/installation/raw_dump.h>
#include <system/installation/rpkg.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace eka2l1 {
    static bool ImGuiButtonToggle(const char *text, ImVec2 size, const bool enable) {
        if (!enable) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        const bool button_result = ImGui::Button(text, size);

        if (!enable) {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }

        return (!enable) ? false : button_result;
    }

    void imgui_debugger::show_install_device() {
        if (device_wizard_state.stage == device_wizard::FINAL_FOR_REAL) {
            device_wizard_state.stage = device_wizard::WELCOME_MESSAGE;
            device_wizard_state.failure = false;

            device_wizard_state.current_rpkg_path.clear();
            device_wizard_state.current_rom_path.clear();

            should_show_install_device_wizard = false;

            std::fill(device_wizard_state.should_continue_temps, device_wizard_state.should_continue_temps + 2,
                false);

            return;
        }

        const std::string install_device_wizard_title = common::get_localised_string(localised_strings,
            "install_device_wizard_title");

        const std::string change_txt = common::get_localised_string(localised_strings, "change");

        ImGui::OpenPopup(install_device_wizard_title.c_str());
        ImGui::SetNextWindowSize(ImVec2(640, 180), ImGuiCond_Once);

        if (ImGui::BeginPopupModal(install_device_wizard_title.c_str())) {
            switch (device_wizard_state.stage) {
            case device_wizard::WELCOME_MESSAGE: {
                const std::string install_device_welcome_msg = common::get_localised_string(localised_strings,
                    "install_device_wizard_welcome_msg");

                /* Currently ImGui does not detect newline from the final
                formatted string but only in the type format string (first
                argument), so we have to do this */
                ImGui::TextWrapped(install_device_welcome_msg.c_str());
                device_wizard_state.should_continue = true;
                break;
            }

            case device_wizard::SPECIFY_FILES: {
                std::map<device_wizard::installation_type, std::string> installation_type_strings = {
                    { device_wizard::INSTALLATION_TYPE_RAW_DUMP, common::get_localised_string(localised_strings, "install_device_wizard_raw_dump_type_name") },
                    { device_wizard::INSTALLATION_TYPE_RPKG, common::get_localised_string(localised_strings, "install_device_wizard_rpkg_type_name") }
                };

                const std::string choose_msg = common::get_localised_string(localised_strings, "install_device_choose_method_msg");

                if (ImGui::BeginCombo(choose_msg.c_str(), installation_type_strings[device_wizard_state.device_install_type].c_str())) {
                    for (auto &installation: installation_type_strings) {
                        if (ImGui::Selectable(installation.second.c_str(), installation.first == device_wizard_state.device_install_type)) {
                            device_wizard_state.device_install_type = installation.first;
                        }
                    }

                    ImGui::EndCombo();
                }
                
                std::string text_to_instruct;
                bool should_choose_folder = false;

                switch (device_wizard_state.device_install_type) {
                case device_wizard::INSTALLATION_TYPE_RPKG:
                    text_to_instruct = common::get_localised_string(localised_strings, "install_device_rpkg_choose_msg");
                    break;

                case device_wizard::INSTALLATION_TYPE_RAW_DUMP:
                    text_to_instruct = common::get_localised_string(localised_strings, "install_device_raw_dump_choose_msg");
                    should_choose_folder = true;
                    break;
                }

                ImGui::TextWrapped("%s", text_to_instruct.c_str());
                ImGui::InputText("##RPKGPath", device_wizard_state.current_rpkg_path.data(),
                    device_wizard_state.current_rpkg_path.size(), ImGuiInputTextFlags_ReadOnly);

                ImGui::SameLine();

                const std::string change_btn_1 = change_txt + "##1";

                if (ImGui::Button(change_btn_1.c_str())) {
                    on_pause_toogle(true);

                    drivers::open_native_dialog(sys->get_graphics_driver(), "rpkg", [&](const char *result) {
                        device_wizard_state.current_rpkg_path = result;
                        device_wizard_state.should_continue_temps[0] = eka2l1::exists(result);
                        device_wizard_state.should_continue = (device_wizard_state.should_continue_temps[1]
                            && eka2l1::exists(result));
                    }, should_choose_folder);

                    should_pause = false;
                    on_pause_toogle(false);
                }

                ImGui::Separator();

                const std::string choose_rom_msg = common::get_localised_string(localised_strings, "install_device_rom_choose_msg");

                ImGui::TextWrapped("%s", choose_rom_msg.c_str());
                ImGui::InputText("##ROMPath", device_wizard_state.current_rom_path.data(),
                    device_wizard_state.current_rom_path.size(), ImGuiInputTextFlags_ReadOnly);

                ImGui::SameLine();

                const std::string change_btn_2 = change_txt + "##2";

                if (ImGui::Button(change_btn_2.c_str())) {
                    on_pause_toogle(true);

                    drivers::open_native_dialog(sys->get_graphics_driver(), "rom", [&](const char *result) {
                        device_wizard_state.current_rom_path = result;
                        device_wizard_state.should_continue_temps[1] = eka2l1::exists(result);
                        device_wizard_state.should_continue = (device_wizard_state.should_continue_temps[0]
                            && eka2l1::exists(result));
                    });

                    should_pause = false;
                    on_pause_toogle(false);
                }


                break;
            }

            case device_wizard::INSTALL: {
                const std::string wait_msg = common::get_localised_string(localised_strings, "install_device_wait_install_msg");
                ImGui::TextWrapped("%s", wait_msg.c_str());

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

                bool extract_rpkg_state = device_wizard_state.extract_rpkg_done.load();
                bool copy_rom_state = device_wizard_state.copy_rom_done.load();

                std::string method_text;

                switch (device_wizard_state.device_install_type) {
                case device_wizard::INSTALLATION_TYPE_RPKG:
                    method_text = common::get_localised_string(localised_strings, "install_device_progress_rpkg_msg");
                    break;

                case device_wizard::INSTALLATION_TYPE_RAW_DUMP:
                    method_text = fmt::format(common::get_localised_string(localised_strings, "install_device_progress_raw_dump_msg"),
                        device_wizard_state.progress_tracker.load());

                    break;
                }

                ImGui::Checkbox(method_text.c_str(), &extract_rpkg_state);
                
                const std::string rom_copy_msg = common::get_localised_string(localised_strings, "install_device_progress_rom_msg");
                ImGui::Checkbox(rom_copy_msg.c_str(), &copy_rom_state);

                if (device_wizard_state.failure) {
                    const std::string wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_install_failure_msg");
                    ImGui::TextWrapped("%s", wrong_happening_msg.c_str());
                    device_wizard_state.should_continue = true;
                }

                ImGui::PopItemFlag();

                break;
            }

            case device_wizard::ENDING: {
                const std::string thank_msg_1 = common::get_localised_string(localised_strings, "install_device_final_msg_1");
                const std::string thank_msg_2 = common::get_localised_string(localised_strings, "install_device_final_msg_2");

                ImGui::TextWrapped("%s", thank_msg_1.c_str());
                ImGui::TextWrapped("%s", thank_msg_2.c_str());

                if (device_wizard_state.install_thread) {
                    device_wizard_state.install_thread->join();
                    device_wizard_state.install_thread.reset();
                }

                device_wizard_state.should_continue = true;
                break;
            }

            default:
                break;
            }

            ImGui::NewLine();
            ImGui::NewLine();

            // Align to center!
            const std::string ok_str = common::get_localised_string(localised_strings, "ok");
            const std::string yes_str = common::get_localised_string(localised_strings, "continue");
            const std::string no_str = common::get_localised_string(localised_strings, "cancel");

            const float yes_str_width = ImGui::CalcTextSize(yes_str.c_str()).x;
            const float no_str_width = ImGui::CalcTextSize(no_str.c_str()).x;
            const float ok_str_width = ImGui::CalcTextSize(ok_str.c_str()).x + 10.0f;

            ImVec2 yes_btn_size(yes_str_width + 10.0f, ImGui::GetFontSize() + 10.0f);
            ImVec2 no_btn_size(no_str_width + 10.0f, ImGui::GetFontSize() + 10.0f);
            ImVec2 ok_btn_size(ok_str_width + 10.0f, ImGui::GetFontSize() + 10.0f);

            if (device_wizard_state.stage == device_wizard::ENDING) {
                ImGui::SameLine((ImGui::GetWindowSize().x - ok_btn_size.x) / 2);

                if (ImGui::Button(ok_str.c_str(), ok_btn_size)) {
                    should_show_install_device_wizard = false;
                    device_wizard_state.stage = device_wizard::FINAL_FOR_REAL;
                }
            } else {
                ImGui::SameLine((ImGui::GetWindowSize().x - (yes_str_width + no_str_width)) / 2);

                if (ImGuiButtonToggle(yes_str.c_str(), yes_btn_size, device_wizard_state.should_continue)) {
                    device_wizard_state.stage = static_cast<device_wizard::device_wizard_stage>(static_cast<int>(device_wizard_state.stage) + 1);
                    device_wizard_state.should_continue = false;

                    if (device_wizard_state.stage == device_wizard::INSTALL) {
                        device_manager *manager = sys->get_device_manager();

                        device_wizard_state.install_thread = std::make_unique<std::thread>([](
                                                                                               device_manager *mngr, device_wizard *wizard, config::state *conf) {
                            std::string firmware_code;
                            
                            wizard->progress_tracker = 0;
                            wizard->extract_rpkg_done = false;
                            wizard->copy_rom_done = false;

                            bool result = false;
                            std::string root_z_path = add_path(conf->storage, "drives/z/");

                            switch (wizard->device_install_type) {
                            case device_wizard::INSTALLATION_TYPE_RPKG:
                                result = eka2l1::loader::install_rpkg(mngr, wizard->current_rpkg_path, root_z_path, firmware_code, wizard->progress_tracker);
                                break;

                            case device_wizard::INSTALLATION_TYPE_RAW_DUMP:
                                result = eka2l1::loader::install_raw_dump(mngr, wizard->current_rpkg_path + eka2l1::get_separator(), root_z_path, firmware_code, wizard->progress_tracker);
                                break;

                            default:
                                wizard->failure = true;
                                return;
                            }

                            if (!result) {
                                wizard->failure = true;
                                return;
                            }

                            mngr->save_devices();

                            wizard->extract_rpkg_done = true;

                            const std::string rom_directory = add_path(conf->storage, add_path("roms", firmware_code + "\\"));

                            eka2l1::create_directories(rom_directory);
                            result = common::copy_file(wizard->current_rom_path, add_path(rom_directory, "SYM.ROM"), true);

                            if (!result) {
                                LOG_ERROR(FRONTEND_UI, "Unable to copy ROM to target ROM directory!");
                                wizard->copy_rom_done = false;
                                return;
                            }

                            wizard->copy_rom_done = true;
                            wizard->should_continue = true;
                        },
                            manager, &device_wizard_state, conf);
                    }
                }

                ImGui::SameLine();

                if ((device_wizard_state.stage != device_wizard::INSTALL) && ImGui::Button(no_str.c_str(), no_btn_size)) {
                    should_show_install_device_wizard = false;
                    device_wizard_state.stage = device_wizard::WELCOME_MESSAGE;
                }
            }

            ImGui::EndPopup();
        }
    }
}