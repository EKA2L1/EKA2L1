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

#include <system/installation/firmware.h>
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

    void imgui_debugger::show_device_installer_choose_variant_popup() {
        const std::string installer_popup_title = common::get_localised_string(localised_strings, "install_device_choose_variant_popup_title");
        ImGui::OpenPopup(installer_popup_title.c_str());

        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x / 4, io.DisplaySize.y / 3));

        if (ImGui::BeginPopupModal(installer_popup_title.c_str())) {
            for (std::size_t i = 0; i < device_install_variant_list->size(); i++) {
                const std::string &name_var = device_install_variant_list->at(i);
                if (ImGui::Selectable(name_var.c_str(), (device_install_variant_index == static_cast<int>(i)))) {
                    device_install_variant_index = static_cast<int>(i);
                }
            }

            ImGui::NewLine();

            const float button_size = 30.0f;
            float total_width_button_occupied = button_size * 2 + 4.0f;

            ImGui::SameLine((ImGui::GetWindowWidth() - total_width_button_occupied) / 2);

            const std::string ok_str = common::get_localised_string(localised_strings, "ok");
            const std::string cancel_str = common::get_localised_string(localised_strings, "cancel");

            if (ImGui::Button(ok_str.c_str())) {
                installer_cond.notify_one();
                should_device_install_wizard_display_variant_select = false;
            }

            ImGui::SameLine();

            if (ImGui::Button(cancel_str.c_str())) {
                device_install_variant_index = -1;
                installer_cond.notify_one();
                should_device_install_wizard_display_variant_select = false;
            }

            ImGui::EndPopup();
        }
    }

    void imgui_debugger::show_install_device() {
        if (device_wizard_state.stage == device_wizard::FINAL_FOR_REAL) {
            device_wizard_state.stage = device_wizard::WELCOME_MESSAGE;
            device_wizard_state.failure = device_installation_none;

            device_wizard_state.current_rpkg_path.clear();
            device_wizard_state.current_rom_or_vpl_path.clear();

            should_show_install_device_wizard = false;

            device_wizard_state.both_exist[0] = false;
            device_wizard_state.both_exist[1] = false;

            return;
        }

        const std::string install_device_wizard_title = common::get_localised_string(localised_strings,
            "install_device_wizard_title");

        const std::string change_txt = common::get_localised_string(localised_strings, "change");

        ImGui::OpenPopup(install_device_wizard_title.c_str());
        ImGui::SetNextWindowSize(ImVec2(640, 180), ImGuiCond_Once);

        auto check_need_additional_rpkg = [](device_wizard &state) {
            state.both_exist[0] = eka2l1::exists(state.current_rom_or_vpl_path);
            state.both_exist[1] = true;
            
            if (state.device_install_type == device_wizard::INSTALLATION_TYPE_DEVICE_DUMP) {
                if (loader::should_install_requires_additional_rpkg(state.current_rom_or_vpl_path)) {
                    state.need_extra_rpkg = true;
                    state.both_exist[1] = eka2l1::exists(state.current_rpkg_path);
                } else {
                    state.need_extra_rpkg = false;
                    state.both_exist[1] = true;
                }
            }

            state.should_continue = (state.both_exist[0] && state.both_exist[1]);
        };

        if (ImGui::BeginPopupModal(install_device_wizard_title.c_str())) {
            switch (device_wizard_state.stage) {
            case device_wizard::WELCOME_MESSAGE: {
                const std::string install_device_welcome_msg = common::get_localised_string(localised_strings,
                    "install_device_wizard_welcome_msg");

                /* Currently ImGui does not detect newline from the final
                formatted string but only in the type format string (first
                argument), so we have to do this */
                ImGui::TextWrapped(install_device_welcome_msg.c_str());

                device_wizard_state.need_extra_rpkg = false;
                check_need_additional_rpkg(device_wizard_state);

                device_wizard_state.should_continue = true;

                break;
            }

            case device_wizard::SPECIFY_FILE: {
                std::map<device_wizard::installation_type, std::string> installation_type_strings = {
                    { device_wizard::INSTALLATION_TYPE_DEVICE_DUMP, common::get_localised_string(localised_strings, "install_device_wizard_device_dump_type_name") },
                    { device_wizard::INSTALLATION_TYPE_FIRMWARE, common::get_localised_string(localised_strings, "install_device_wizard_firmware_type_name") }
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

                if (device_wizard_state.device_install_type == device_wizard::INSTALLATION_TYPE_DEVICE_DUMP) {
                    ImGui::Separator();
                    const std::string rpkg_note = common::get_localised_string(localised_strings, "install_device_note_may_need_rpkg");
                    ImGui::TextWrapped("%s", rpkg_note.c_str());
                }

                ImGui::Separator();
                
                std::string text_to_instruct;
                std::string filter_str;

                bool should_choose_folder = false;

                switch (device_wizard_state.device_install_type) {
                case device_wizard::INSTALLATION_TYPE_FIRMWARE:
                    text_to_instruct = common::get_localised_string(localised_strings, "install_device_firmware_vpl_choose_msg");
                    filter_str = "vpl";
                    break;

                case device_wizard::INSTALLATION_TYPE_DEVICE_DUMP:
                    text_to_instruct = common::get_localised_string(localised_strings, "install_device_rom_choose_msg");
                    filter_str = "rom";
                    break;
                }

                ImGui::TextWrapped("%s", text_to_instruct.c_str());
                ImGui::InputText("##ROMOrVPL", device_wizard_state.current_rom_or_vpl_path.data(),
                    device_wizard_state.current_rom_or_vpl_path.size(), ImGuiInputTextFlags_ReadOnly);

                ImGui::SameLine();

                const std::string change_btn_1 = change_txt + "##1";

                if (ImGui::Button(change_btn_1.c_str())) {
                    on_pause_toogle(true);

                    drivers::open_native_dialog(sys->get_graphics_driver(), filter_str.c_str(), [&](const char *result) {
                        device_wizard_state.current_rom_or_vpl_path = result;
                        check_need_additional_rpkg(device_wizard_state);
                    });

                    should_pause = false;
                    on_pause_toogle(false);
                }

                if (device_wizard_state.need_extra_rpkg) {
                    ImGui::Separator();

                    text_to_instruct = common::get_localised_string(localised_strings, "install_device_rpkg_choose_msg");

                    ImGui::TextWrapped("%s", text_to_instruct.c_str());
                    ImGui::InputText("##RPKG", device_wizard_state.current_rpkg_path.data(),
                        device_wizard_state.current_rpkg_path.size(), ImGuiInputTextFlags_ReadOnly);

                    ImGui::SameLine();

                    const std::string change_btn_2 = change_txt + "##2";

                    if (ImGui::Button(change_btn_2.c_str())) {
                        on_pause_toogle(true);

                        drivers::open_native_dialog(sys->get_graphics_driver(), "rpkg", [&](const char *result) {
                            device_wizard_state.current_rpkg_path = result;
                            device_wizard_state.both_exist[1] = eka2l1::exists(result);

                            device_wizard_state.should_continue = (device_wizard_state.both_exist[0] && device_wizard_state.both_exist[1]);
                        });

                        should_pause = false;
                        on_pause_toogle(false);
                    }
                }

                break;
            }

            case device_wizard::INSTALL: {
                const std::string wait_msg = common::get_localised_string(localised_strings, "install_device_wait_install_msg");
                ImGui::TextWrapped("%s", wait_msg.c_str());

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

                bool s1_done = device_wizard_state.stage_done[0].load();

                std::string method_text;

                switch (device_wizard_state.device_install_type) {
                case device_wizard::INSTALLATION_TYPE_DEVICE_DUMP:
                    if (device_wizard_state.need_extra_rpkg) {
                        method_text = fmt::format(common::get_localised_string(localised_strings, "install_device_progress_rpkg_msg"),
                            device_wizard_state.progress_tracker[0].load());
                    } else {
                        method_text = fmt::format(common::get_localised_string(localised_strings, "install_device_progress_rom_msg"),
                            device_wizard_state.progress_tracker[0].load());
                    }
                    break;

                case device_wizard::INSTALLATION_TYPE_FIRMWARE:
                    method_text = fmt::format(common::get_localised_string(localised_strings, "install_device_progress_fpsx_msg"),
                        device_wizard_state.progress_tracker[0].load());

                    break;

                default:
                    break;
                }

                ImGui::Checkbox(method_text.c_str(), &s1_done);
                if (device_wizard_state.need_extra_rpkg) {
                    const std::string rpkg_state = fmt::format(common::get_localised_string(localised_strings,
                        "install_device_progress_rom_msg"), device_wizard_state.progress_tracker[1].load());

                    bool s2_done = device_wizard_state.stage_done[1].load();
                    ImGui::Checkbox(rpkg_state.c_str(), &s2_done);
                }

                if (device_wizard_state.failure != device_installation_none) {
                    std::string wrong_happening_msg;

                    switch (device_wizard_state.failure.load()) {
                    case device_installation_already_exist:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_already_exist_msg");
                        break;

                    case device_installation_determine_product_failure:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_product_determine_fail_msg");
                        break;

                    case device_installation_rom_fail_to_copy:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_rom_fail_to_copy_msg");
                        break;

                    case device_installation_insufficent:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_rpkg_insufficent_size_msg");
                        break;

                    case device_installation_not_exist:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_rpkg_file_not_found_msg");
                        break;

                    case device_installation_rpkg_corrupt:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_install_rpkg_corrupt_msg");
                        break;

                    case device_installation_rofs_corrupt:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_rofs_corrupt_msg");
                        break;

                    case device_installation_rom_file_corrupt:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_rom_corrupt_msg");
                        break;

                    case device_installation_fpsx_corrupt:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_fpsx_corrupt_msg");
                        break;

                    case device_installation_vpl_file_invalid:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_vpl_invalid_msg");
                        break;

                    default:
                        wrong_happening_msg = common::get_localised_string(localised_strings, "install_device_install_failure_msg");
                        break;
                    }

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

                        device_wizard_state.install_thread = std::make_unique<std::thread>([this](
                                                                                               device_manager *mngr, device_wizard *wizard, config::state *conf) {
                            std::string firmware_code;
                            
                            wizard->progress_tracker[0] = 0;
                            wizard->progress_tracker[1] = 0;
                            wizard->stage_done[0] = false;
                            wizard->stage_done[1] = false;

                            device_installation_error result = device_installation_none;

                            std::string rom_resident_path = add_path(conf->storage, "roms");
                            std::string root_z_path = add_path(conf->storage, "drives/z/");
                            std::string root_c_path = add_path(conf->storage, "drives/c/");
                            std::string root_e_path = add_path(conf->storage, "drives/e/");

                            eka2l1::create_directories(rom_resident_path);

                            switch (wizard->device_install_type) {
                            case device_wizard::INSTALLATION_TYPE_DEVICE_DUMP:
                                if (wizard->need_extra_rpkg)
                                    result = eka2l1::loader::install_rpkg(mngr, wizard->current_rpkg_path, root_z_path, firmware_code, wizard->progress_tracker[0], 100);
                                else
                                    result = eka2l1::loader::install_rom(mngr, wizard->current_rom_or_vpl_path, rom_resident_path, root_z_path, wizard->progress_tracker[0]);
                                break;

                            case device_wizard::INSTALLATION_TYPE_FIRMWARE:
                                result = eka2l1::install_firmware(mngr, wizard->current_rom_or_vpl_path, root_c_path, root_e_path, root_z_path,
                                    rom_resident_path, [&](const std::vector<std::string> &variants) {
                                        this->device_install_variant_list = &variants;
                                        this->device_install_variant_index = 0;
    
                                        this->should_device_install_wizard_display_variant_select = true;

                                        std::unique_lock<std::mutex> ul(installer_mut);
                                        this->installer_cond.wait(ul);

                                        // Normally debug thread won't touch this
                                        this->should_device_install_wizard_display_variant_select = false;
                                        return this->device_install_variant_index;
                                    }, wizard->progress_tracker[0]);
                                break;

                            default:
                                wizard->failure = device_installation_general_failure;
                                return;
                            }

                            if (result != device_installation_none) {
                                wizard->failure = result;
                                return;
                            }

                            mngr->save_devices();
                            wizard->stage_done[0] = true;

                            if (wizard->need_extra_rpkg) {
                                const std::string rom_directory = add_path(rom_resident_path, firmware_code + "\\");

                                eka2l1::create_directories(rom_directory);

                                if (!common::copy_file(wizard->current_rom_or_vpl_path, add_path(rom_directory, "SYM.ROM"), true)) {
                                    LOG_ERROR(FRONTEND_UI, "Unable to copy ROM to target ROM directory!");
                                    wizard->stage_done[1] = false;
                                    return;
                                }
                            }

                            wizard->stage_done[1] = true;
                            wizard->progress_tracker[1] = 100;

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
