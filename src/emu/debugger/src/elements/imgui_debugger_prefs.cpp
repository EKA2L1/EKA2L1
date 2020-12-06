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
#include <debugger/renderer/renderer.h>

#include <drivers/itc.h>

#include <config/app_settings.h>
#include <config/config.h>
#include <system/devices.h>
#include <package/manager.h>

#include <common/cvt.h>
#include <common/language.h>

#include <cpu/arm_utils.h>
#include <system/epoc.h>

#include <kernel/kernel.h>
#include <kernel/property.h>

#include <services/applist/applist.h>
#include <services/window/window.h>
#include <services/ui/cap/oom_app.h>

#include <utils/consts.h>
#include <utils/locale.h>
#include <utils/system.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace eka2l1 {
    void imgui_debugger::show_pref_personalisation() {
        ImGui::AlignTextToFramePadding();

        const std::string background_str = common::get_localised_string(localised_strings, "pref_personalize_background_string");

        ImGui::Text(background_str.c_str());
        ImGui::Separator();

        const std::string path_str = common::get_localised_string(localised_strings, "path");

        ImGui::Text("%s         ", path_str.c_str());
        ImGui::SameLine();
        ImGui::InputText("##Background", conf->bkg_path.data(), conf->bkg_path.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();

        const std::string change_str = common::get_localised_string(localised_strings, "change");

        if (ImGui::Button(change_str.c_str())) {
            on_pause_toogle(true);

            drivers::open_native_dialog(
                sys->get_graphics_driver(), "png,jpg,bmp", [&](const char *result) {
                    conf->bkg_path = result;
                    renderer->change_background(result);

                    conf->serialize();
                },
                false);

            should_pause = false;
            on_pause_toogle(false);
        }

        const std::string transparency_str = common::get_localised_string(localised_strings, "pref_personalize_transparency_string");
        const std::string ui_scale_str = common::get_localised_string(localised_strings, "pref_personalize_ui_scale_string");
        
        ImGui::Text("%s ", transparency_str.c_str());
        ImGui::SameLine();
        ImGui::SliderInt("##BackgroundTransparency", &conf->bkg_transparency, 0, 255);

        ImGui::Text("%s     ", ui_scale_str.c_str());
        ImGui::SameLine();
        const bool ret = ImGui::InputFloat("##UIScale", &conf->ui_scale, 0.1f);
        if (ret && conf->ui_scale <= 1e-6) {
            conf->ui_scale = 0.5;
        }

        ImGui::Separator();
    }

    language imgui_debugger::get_language_from_property(service::property *lang_prop) {
        auto current_lang = lang_prop->get_pkg<epoc::locale_language>();

        if (!current_lang) {
            return language::en;
        }

        return static_cast<language>(current_lang->language);
    }

    void imgui_debugger::set_language_to_property(const language new_one) {
        kernel_system *kern = sys->get_kernel_system();

        property_ptr lang_prop = kern->get_prop(epoc::SYS_CATEGORY, epoc::LOCALE_LANG_KEY);
        auto current_lang = lang_prop->get_pkg<epoc::locale_language>();

        if (!current_lang) {
            return;
        }

        current_lang->language = static_cast<epoc::language>(new_one);
        lang_prop->set<epoc::locale_language>(current_lang.value());
    }

    void imgui_debugger::show_pref_general() {
        const float col3 = ImGui::GetWindowSize().x / 3;

        auto draw_path_change = [&](const char *title, const char *button, std::string &dat) -> bool {
            ImGui::Text("%s", title);
            ImGui::SameLine(col3);

            ImGui::PushItemWidth(col3 * 2 - 75);
            ImGui::InputText("##Path", dat.data(), dat.size(), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();

            ImGui::SameLine(col3 * 3 - 65);
            ImGui::PushItemWidth(30);

            bool change = false;

            if (ImGui::Button(button)) {
                on_pause_toogle(true);

                drivers::open_native_dialog(
                    sys->get_graphics_driver(),
                    "", [&](const char *res) {
                        dat = res;
                        change = true;
                    },
                    true);

                should_pause = false;
                on_pause_toogle(false);
            }

            ImGui::PopItemWidth();
            return change;
        };

        const std::string change_str = common::get_localised_string(localised_strings, "change");
        const std::string change_btn_1 = change_str + "##1";
        const std::string data_storage_str = common::get_localised_string(localised_strings, "pref_general_data_storage_string");

        if (draw_path_change(data_storage_str.c_str(), change_btn_1.c_str(), conf->storage)) {
            device_manager *dvc_mngr = sys->get_device_manager();
            dvc_mngr->load_devices();

            should_notify_reset_for_big_change = true;
        }

        const std::string deb_title = common::get_localised_string(localised_strings, "pref_general_debugging_string");

        ImGui::Text("%s", deb_title.c_str());
        ImGui::Separator();

        const float col2 = ImGui::GetWindowSize().x / 2;

        const std::string cpu_read_str = common::get_localised_string(localised_strings, "pref_general_debugging_cpu_read_checkbox_title");
        const std::string cpu_write_str = common::get_localised_string(localised_strings, "pref_general_debugging_cpu_write_checkbox_title");
        const std::string cpu_step_str = common::get_localised_string(localised_strings, "pref_general_debugging_cpu_step_checkbox_title");
        const std::string ipc_str = common::get_localised_string(localised_strings, "pref_general_debugging_ipc_checkbox_title");
        const std::string system_calls_str = common::get_localised_string(localised_strings, "pref_general_debugging_system_calls_checkbox_title");
        const std::string accurate_ipc_timing_str = common::get_localised_string(localised_strings, "pref_general_debugging_ait_checkbox_title");
        const std::string enable_btrace_str = common::get_localised_string(localised_strings, "pref_general_debugging_enable_btrace_checkbox_title");
        
        ImGui::Checkbox(cpu_read_str.c_str(), &conf->log_read);
        ImGui::SameLine(col2);
        ImGui::Checkbox(cpu_write_str.c_str(), &conf->log_write);

        ImGui::Checkbox(ipc_str.c_str(), &conf->log_ipc);
        ImGui::SameLine(col2);
        ImGui::Checkbox("Symbian API", &conf->log_passed);

        ImGui::Checkbox(system_calls_str.c_str(), &conf->log_svc);
        ImGui::SameLine(col2);
        ImGui::Checkbox(accurate_ipc_timing_str.c_str(), &conf->accurate_ipc_timing);
        if (ImGui::IsItemHovered()) {
            const std::string ait_tt = common::get_localised_string(localised_strings, "pref_general_debugging_ait_tooltip_msg");
            ImGui::SetTooltip("%s", ait_tt.c_str());
        }

        ImGui::Checkbox(enable_btrace_str.c_str(), &conf->enable_btrace);

        if (ImGui::IsItemHovered()) {
            const std::string btrace_tt = common::get_localised_string(localised_strings, "pref_general_debugging_btrace_tooltip_msg");
            ImGui::SetTooltip("%s", btrace_tt.c_str());
        }

        ImGui::SameLine(col2);

        bool do_we_step = conf->stepping.load();
        ImGui::Checkbox(cpu_step_str.c_str(), &do_we_step);
        conf->stepping.store(do_we_step);

        if (ImGui::IsItemHovered()) {
            const std::string cpu_step_tt = common::get_localised_string(localised_strings, "pref_general_debugging_cpu_step_checkbox_tooltip_msg");
            ImGui::SetTooltip("%s", cpu_step_tt.c_str());
        }

        const std::string utils_sect_title = common::get_localised_string(localised_strings, "pref_general_utilities_string");
        const std::string utils_hide_mouse_str = common::get_localised_string(localised_strings, "pref_general_utilities_hide_cursor_in_screen_space_checkbox_title");
        const std::string nearest_filtering_str = common::get_localised_string(localised_strings, "pref_general_utilities_nearest_neighbor_filtering_msg");
        const std::string nearest_filtering_tt = common::get_localised_string(localised_strings, "pref_general_utilities_nearest_neighbor_filtering_tooltip");
        const std::string integer_scaling_str = common::get_localised_string(localised_strings, "pref_general_utilities_integer_scaling_msg");
        const std::string integer_scaling_tt = common::get_localised_string(localised_strings, "pref_general_utilities_integer_scaling_tooltip");

        ImGui::NewLine();
        ImGui::Text("%s", utils_sect_title.c_str());
        ImGui::Separator();

        ImGui::Checkbox(utils_hide_mouse_str.c_str(), &conf->hide_mouse_in_screen_space);
        if (ImGui::IsItemHovered()) {
            const std::string mouse_hide_tt = common::get_localised_string(localised_strings, "pref_general_utilities_hide_cursor_in_screen_space_tooltip_msg");
            ImGui::SetTooltip("%s", mouse_hide_tt.c_str());
        }

        const bool previous_filter = conf->nearest_neighbor_filtering;
        ImGui::Checkbox(nearest_filtering_str.c_str(), &conf->nearest_neighbor_filtering);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", nearest_filtering_tt.c_str());
        }

        ImGui::Checkbox(integer_scaling_str.c_str(), &conf->integer_scaling);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", integer_scaling_tt.c_str());
        }

        const std::map<int, std::string> AVAIL_LANG_LIST = {
            { static_cast<int>(language::en), common::get_localised_string(localised_strings, "lang_name_en") },
            { static_cast<int>(language::de), common::get_localised_string(localised_strings, "lang_name_de") },
            { static_cast<int>(language::bp), common::get_localised_string(localised_strings, "lang_name_bp") },
            { static_cast<int>(language::es), common::get_localised_string(localised_strings, "lang_name_es") },
            { static_cast<int>(language::it), common::get_localised_string(localised_strings, "lang_name_it") },
            { static_cast<int>(language::vi), common::get_localised_string(localised_strings, "lang_name_vi") },
            { static_cast<int>(language::ru), common::get_localised_string(localised_strings, "lang_name_ru") },
            { static_cast<int>(language::ro), common::get_localised_string(localised_strings, "lang_name_ro") },
            { static_cast<int>(language::fr), common::get_localised_string(localised_strings, "lang_name_fr") },
            { static_cast<int>(language::fi), common::get_localised_string(localised_strings, "lang_name_fi") },
            { static_cast<int>(language::zh), common::get_localised_string(localised_strings, "lang_name_zh") },
            { static_cast<int>(language::pl), common::get_localised_string(localised_strings, "lang_name_pl") },
            { static_cast<int>(language::uk), common::get_localised_string(localised_strings, "lang_name_uk") },
            { static_cast<int>(language::tr), common::get_localised_string(localised_strings, "lang_name_tr") },
            { static_cast<int>(language::az), common::get_localised_string(localised_strings, "lang_name_az") },
            { static_cast<int>(language::bn), common::get_localised_string(localised_strings, "lang_name_bn") },
            { static_cast<int>(language::id), common::get_localised_string(localised_strings, "lang_name_id") }
        };
        
        ImGui::NewLine();
        const std::string lang_op = common::get_localised_string(localised_strings, "pref_system_language_option_name");

        ImGui::Text("%s", lang_op.c_str());
        ImGui::SameLine(col3);
        ImGui::PushItemWidth(col3 * 2 - 10);

        const auto default_current_lang = AVAIL_LANG_LIST.find(conf->emulator_language);
        const std::string default_current_lang_str = (default_current_lang == AVAIL_LANG_LIST.end()) ? ""
            : default_current_lang->second;

        if (ImGui::BeginCombo("##EmuLangCombo", default_current_lang_str.c_str())) {
            for (const auto &lang: AVAIL_LANG_LIST) {
                if (ImGui::Selectable(lang.second.c_str())) {
                    conf->emulator_language = lang.first;
                    localised_strings = common::get_localised_string_table("resources", "strings.xml",
                        static_cast<language>(conf->emulator_language));

                    app_setting_msg.clear();
                }
            }

            ImGui::EndCombo();
        }
    };

    void imgui_debugger::show_pref_system() {
        const float col2 = ImGui::GetWindowSize().x / 2;

        ImGui::NewLine();

        const std::string general_tit = common::get_localised_string(localised_strings, "pref_general_title");
        ImGui::Text("%s", general_tit.c_str());
        ImGui::Separator();

        const std::string cpu_op = common::get_localised_string(localised_strings, "pref_system_cpu_option_name");
        ImGui::Text("%s", cpu_op.c_str());
        ImGui::SameLine(col2);
        ImGui::PushItemWidth(col2 - 10);

        if (ImGui::BeginCombo("##CPUCombo", arm::arm_emulator_type_to_string(sys->get_cpu_executor_type()))) {
            if (ImGui::Selectable("Dynarmic")) {
                conf->cpu_backend = "Dynarmic";
                sys->set_cpu_executor_type(arm_emulator_type::dynarmic);
                conf->serialize();
            }

            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();

        const std::string device_op = common::get_localised_string(localised_strings, "pref_system_device_option_name");
        ImGui::Text("%s", device_op.c_str());
        ImGui::SameLine(col2);
        ImGui::PushItemWidth(col2 - 10);

        device_manager *mngr = sys->get_device_manager();
        mngr->lock.lock();

        auto &dvcs = mngr->get_devices();

        if (!dvcs.empty() && (conf->device >= dvcs.size())) {
            const std::string device_not_found_msg_log = common::get_localised_string(localised_strings, "pref_system_device_not_found_msg");

            LOG_WARN("{}", device_not_found_msg_log);
            conf->device = 0;
        }

        const std::string preview_info = dvcs.empty() ? common::get_localised_string(localised_strings, "pref_system_no_device_present_str")
            : dvcs[conf->device].model + " (" + dvcs[conf->device].firmware_code + ")";

        auto set_language_current = [&](const language lang) {
            conf->language = static_cast<int>(lang);
            sys->set_system_language(lang);
            set_language_to_property(lang);

            conf->serialize();
        };

        if (ImGui::BeginCombo("##Devicescombo", preview_info.c_str())) {
            for (std::size_t i = 0; i < dvcs.size(); i++) {
                const std::string device_info = dvcs[i].model + " (" + dvcs[i].firmware_code + ")";
                if (ImGui::Selectable(device_info.c_str())) {
                    if (conf->device != i) {
                        // Check if the language currently in config exists in the new device
                        if (std::find(dvcs[i].languages.begin(), dvcs[i].languages.end(), conf->language) == dvcs[i].languages.end()) {
                            set_language_current(static_cast<language>(dvcs[i].default_language_code));
                        }

                        conf->device = static_cast<int>(i);
                        conf->serialize();

                        // Hamburger!!!!!!!! :L
                        mngr->lock.unlock();
                        in_reset = true;

                        sys->set_device(static_cast<std::uint8_t>(i));

                        in_reset = false;
                        mngr->lock.lock();

                        should_notify_reset_for_big_change = true;
                    }
                }
            }

            ImGui::EndCombo();
        }

        if (!dvcs.empty()) {
            ImGui::PopItemWidth();

            const std::string lang_str = common::get_localised_string(localised_strings, "pref_system_language_option_name");

            ImGui::Text("%s", lang_str.c_str());
            ImGui::SameLine(col2);
            ImGui::PushItemWidth(col2 - 10);

            auto &dvc = dvcs[conf->device];

            if (conf->language == -1) {
                set_language_current(static_cast<language>(dvc.default_language_code));
            }

            const std::string lang_preview = common::get_language_name_by_code(conf->language);

            if (ImGui::BeginCombo("##Languagesscombo", lang_preview.c_str())) {
                for (std::size_t i = 0; i < dvc.languages.size(); i++) {
                    const std::string lang_name = common::get_language_name_by_code(dvc.languages[i]);
                    if (ImGui::Selectable(lang_name.c_str())) {
                        set_language_current(static_cast<language>(dvc.languages[i]));
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();

            kernel_system *kern = sys->get_kernel_system();

            const std::string rt_acc_str = common::get_localised_string(localised_strings, "pref_system_real_time_accuracy_str");
            ImGui::LabelText("##GrayedRTOSLabel", "%s", rt_acc_str.c_str());

            if (ImGui::IsItemHovered()) {
                const std::string rt_acc_tt_1 = common::get_localised_string(localised_strings, "pref_system_real_time_accuracy_tooltip_msg_1");
                const std::string rt_acc_tt_2 = common::get_localised_string(localised_strings, "pref_system_real_time_accuracy_tooltip_msg_2");
                
                ImGui::SetTooltip("%s\n%s", rt_acc_tt_1.c_str(), rt_acc_tt_2.c_str());
            }

            ImGui::SameLine(col2);
            ImGui::PushItemWidth(col2 - 10);

            std::map<realtime_level, std::string> RTOS_LEVEL_NAME_AND_LEVEL = {
                { realtime_level_low, common::get_localised_string(localised_strings, "pref_system_real_time_accuracy_level_name_low") },
                { realtime_level_mid, common::get_localised_string(localised_strings, "pref_system_real_time_accuracy_level_name_mid") },
                { realtime_level_high, common::get_localised_string(localised_strings, "pref_system_real_time_accuracy_level_name_high") }
            };

            ntimer *timing = kern->get_ntimer();
            if (ImGui::BeginCombo("##RTOSLevel", RTOS_LEVEL_NAME_AND_LEVEL[timing->get_realtime_level()].c_str())) {
                for (const auto &lvl: RTOS_LEVEL_NAME_AND_LEVEL) {
                    if (ImGui::Selectable(lvl.second.c_str())) {
                        timing->set_realtime_level(lvl.first);
                        conf->rtos_level = get_string_of_realtime_level(lvl.first);
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();
            ImGui::NewLine();

            io_system *io = sys->get_io_system();

            if (!should_disable_validate_drive) {
                kern->lock();

                if (!kern->threads_.empty()) {
                    should_disable_validate_drive = true;
                }

                kern->unlock();
            }

            if (should_disable_validate_drive) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }

            const std::string validate_device_str = common::get_localised_string(localised_strings,
                "pref_system_validate_device_btn_title");

            if (ImGui::Button(validate_device_str.c_str())) {
                LOG_INFO("This might take sometimes! Please wait... The UI is frozen while this is being done.");
                sys->validate_current_device();
            }

            if (should_disable_validate_drive) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }
            
            const std::string rescan_device_str = common::get_localised_string(localised_strings,
                "pref_system_rescan_devices_title");

            ImGui::SameLine();
            if (ImGui::Button(rescan_device_str.c_str())) {
                mngr->lock.unlock();
                sys->rescan_devices(drive_z);
                mngr->lock.lock();
            }

            if (ImGui::IsItemHovered()) {
                const std::string tooltip = common::get_localised_string(localised_strings, "pref_system_validate_device_tooltip");
                std::string tooltip_disabled = "";

                if (should_disable_validate_drive) {
                    tooltip_disabled = common::get_localised_string(localised_strings, "pref_system_validate_device_disabled_tooltip");
                }

                ImGui::SetTooltip("%s\n%s", tooltip.c_str(), tooltip_disabled.c_str());
            }

            const std::string app_setting_str = common::get_localised_string(localised_strings, "pref_system_app_setting_sect_name");

            ImGui::NewLine();
            ImGui::Text("%s", app_setting_str.c_str());

            ImGui::SameLine(col2);
            ImGui::PushItemWidth(col2 - 10);
            
            if (alserv) {
                kernel_system *kern = sys->get_kernel_system();
                akn_running_app_info choosen;

                {
                    kern->lock();

                    auto ui_app_list = eka2l1::get_akn_app_infos(winserv);

                    if (active_app_config) {
                        auto find_res = std::find_if(ui_app_list.begin(), ui_app_list.end(), [=](akn_running_app_info &info) {
                            return info.associated_ == active_app_config;
                        });

                        if (find_res == ui_app_list.end()) {
                            active_app_config = nullptr;
                        } else {
                            choosen = *find_res;
                        }
                    }

                    if (!active_app_config && (!ui_app_list.empty())) {
                        active_app_config = ui_app_list.front().associated_;
                        choosen = ui_app_list.front();

                        app_setting_msg.clear();
                    }

                    std::string preview = active_app_config ? common::ucs2_to_utf8(choosen.app_name_) : "None";

                    if (ImGui::BeginCombo("##AppListCombo", preview.c_str())) {
                        for (auto &ui_app: ui_app_list) {
                            if (!(ui_app.flags_ & akn_running_app_info::FLAG_SYSTEM)) {
                                const std::string select_name = common::ucs2_to_utf8(ui_app.app_name_) + ((ui_app.flags_ & akn_running_app_info::FLAG_CURRENTLY_PLAY) ?
                                    fmt::format(" (Active on screen {})", ui_app.screen_number_) : "");

                                if (ImGui::Selectable(select_name.c_str())) {
                                    active_app_config = ui_app.associated_;
                                    app_setting_msg.clear();
                                }
                            }
                        }

                        ImGui::EndCombo();
                    }

                    kern->unlock();
                }

                ImGui::Separator();

                if (active_app_config) {
                    kern->lock();

                    static const char *TIME_UNIT_NAME = "US";

                    const std::string time_delay_str = common::get_localised_string(localised_strings, "pref_system_time_delay_option_name");

                    ImGui::Text("%s", time_delay_str.c_str());
                    ImGui::SameLine(col2);
                    ImGui::PushItemWidth(col2 - ImGui::CalcTextSize(TIME_UNIT_NAME).x - 15);

                    int last_delay = static_cast<int>(active_app_config->get_time_delay());
                    int current_delay = last_delay;
                    
                    static constexpr int MIN_TIME_DELAY = 0;
                    static constexpr int MAX_TIME_DELAY = 500;

                    if (ImGui::SliderInt(TIME_UNIT_NAME, &current_delay, MIN_TIME_DELAY, MAX_TIME_DELAY)) {
                        if (current_delay != last_delay) {
                            active_app_config->set_time_delay(current_delay);
                        }
                    }

                    ImGui::PopItemWidth();

                    bool last_setting_inherit = active_app_config->get_child_inherit_setting();
                    bool new_setting_inherit = last_setting_inherit;

                    const std::string inherit_str = common::get_localised_string(localised_strings, "pref_system_setting_inheritance_option_name");
                    if (ImGui::Checkbox(inherit_str.c_str(), &new_setting_inherit)) {
                        if (new_setting_inherit != last_setting_inherit) {
                            active_app_config->set_child_inherit_setting(new_setting_inherit);
                        }
                    }

                    if (ImGui::IsItemHovered()) {
                        const std::string inherit_tt = common::get_localised_string(localised_strings, "pref_system_setting_inheritance_tooltip_msg");
                        ImGui::SetTooltip("%s", inherit_tt.c_str());
                    }

                    kern->unlock();

                    const std::string save_the_setting_str = common::get_localised_string(localised_strings, "pref_system_app_setting_save_title");
                    const float save_the_setting_button_size = ImGui::CalcTextSize(save_the_setting_str.c_str()).x + 10.0f;

                    if (!app_setting_msg.empty()) {
                        ImGui::TextColored(GREEN_COLOR, "%s", app_setting_msg.c_str());
                    }

                    ImGui::NewLine();
                    
                    ImGui::SameLine(std::max(0.0f, (ImGui::GetWindowSize().x - save_the_setting_button_size) / 2));
                    ImGui::PushItemWidth(save_the_setting_button_size);

                    if (ImGui::Button(save_the_setting_str.c_str())) {
                        config::app_settings *settings = kern->get_app_settings();

                        config::app_setting to_replace;
                        to_replace.fps = winserv->get_screen(choosen.screen_number_)->refresh_rate;
                        to_replace.child_inherit_setting = active_app_config->get_child_inherit_setting();
                        to_replace.time_delay = active_app_config->get_time_delay();

                        if (settings->add_or_replace_setting(active_app_config->get_uid(), to_replace)) {
                            app_setting_msg = common::get_localised_string(localised_strings, "pref_system_app_setting_save_done_msg");
                            app_setting_msg = fmt::format(app_setting_msg, common::ucs2_to_utf8(choosen.app_name_), choosen.app_uid_);
                        }
                    }

                    ImGui::PopItemWidth();
                }
            } else {
                ImGui::Text("Disabled due to missing components!");
                ImGui::Separator();
            }

            ImGui::PopItemWidth();
        }
    
        mngr->lock.unlock();
    }

    void imgui_debugger::show_pref_control() {
        const std::string key_bind_msg = common::get_localised_string(localised_strings, "pref_control_key_binding_sect_title");
        ImGui::Text("%s", key_bind_msg.c_str());

        ImGui::Separator();
        const float btn_col2 = ImGui::GetWindowSize().x / 4;
        const float btn_col3 = ImGui::GetWindowSize().x / 2;
        const float btn_col4 = ImGui::GetWindowSize().x / 4 * 3;
        for (int i = 0; i < key_binder_state.target_key.size(); i++) {
            if (i % 2 == 1)
                ImGui::SameLine(btn_col3);
            ImGui::Text("%s: ", key_binder_state.target_key_name[i].c_str());
            if (i % 2 == 1) {
                ImGui::SameLine(btn_col4);
            } else {
                ImGui::SameLine(btn_col2);
            }
            if (key_binder_state.need_key[i]) {
                if (!request_key) {
                    request_key = true;
                } else if (key_set) {
                    // fetch key and add to map
                    bool map_set = false;
                    config::keybind new_kb;
                    new_kb.target = key_binder_state.target_key[i];

                    // NOTE: key_binder_state belongs to UI. setting a key_bind_name means setting the display name of binded key
                    // in keybind window.
                    switch (key_evt.type_) {
                    case drivers::input_event_type::key:
                        winserv->input_mapping.key_input_map[key_evt.key_.code_] = key_binder_state.target_key[i];
                        key_binder_state.key_bind_name[new_kb.target] = std::to_string(key_evt.key_.code_);

                        new_kb.source.type = config::KEYBIND_TYPE_KEY;
                        new_kb.source.data.keycode = key_evt.key_.code_;
                        map_set = true;

                        break;

                    case drivers::input_event_type::button:
                        winserv->input_mapping.button_input_map[std::make_pair(key_evt.button_.controller_, key_evt.button_.button_)] = key_binder_state.target_key[i];
                        key_binder_state.key_bind_name[new_kb.target] = std::to_string(key_evt.button_.controller_) + ":" + std::to_string(key_evt.button_.button_);
                        
                        new_kb.source.type = config::KEYBIND_TYPE_CONTROLLER;
                        new_kb.source.data.button.controller_id = key_evt.button_.controller_;
                        new_kb.source.data.button.button_id = key_evt.button_.button_;
                        map_set = true;
                        
                        break;

                    case drivers::input_event_type::touch: {
                        // Use the same map for mouse
                        const std::uint32_t MOUSE_KEYCODE = epoc::KEYBIND_TYPE_MOUSE_CODE_BASE + key_evt.mouse_.button_;

                        winserv->input_mapping.key_input_map[MOUSE_KEYCODE] = key_binder_state.target_key[i];
                        key_binder_state.key_bind_name[new_kb.target] = "M" + std::to_string(key_evt.mouse_.button_);

                        new_kb.source.type = config::KEYBIND_TYPE_MOUSE;
                        new_kb.source.data.keycode = key_evt.mouse_.button_;
                        map_set = true;

                        break;
                    }

                    default:
                        break;
                    }

                    if (map_set) {
                        key_binder_state.need_key[i] = false;
                        request_key = false;
                        key_set = false;
                    }

                    bool exist_in_config = false;
                    for (auto &kb : conf->keybinds) {
                        if (kb.target == key_binder_state.target_key[i]) {
                            kb = new_kb;
                            exist_in_config = true;
                            break;
                        }
                    }

                    if (!exist_in_config) {
                        conf->keybinds.emplace_back(new_kb);
                    }
                }

                const std::string wait_key_msg = common::get_localised_string(localised_strings, "pref_control_wait_for_key_string");
                ImGui::Button(wait_key_msg.c_str());
            } else {
                if (ImGui::Button(key_binder_state.key_bind_name[key_binder_state.target_key[i]].c_str())) {
                    key_binder_state.need_key[i] = true;
                    request_key = true;
                }
            }
        }
    }
    
    void imgui_debugger::show_pref_hal() {
        const float col6 = ImGui::GetWindowSize().x / 6;

        static const char *BATTERY_LEVEL_STRS[] = {
            "0", "1", "2", "3",
            "4", "5", "6", "7"
        };

        if (!oom->get_eik_server()) {
            oom->init(sys->get_kernel_system(), sys->get_io_system(), sys->get_device_manager());
        }

        epoc::cap::eik_server *eik = oom->get_eik_server();

        const std::string bat_level_str = common::get_localised_string(localised_strings, "pref_hal_battery_level_string");
        const std::string sig_level_str = common::get_localised_string(localised_strings, "pref_hal_signal_level_string");

        ImGui::Text("%s", bat_level_str.c_str());
        ImGui::SameLine(col6 + 10);

        ImGui::PushItemWidth(col6 * 2 - 10);

        if (ImGui::BeginCombo("##BatteryLevel", BATTERY_LEVEL_STRS[eik->get_pane_maintainer()->get_battery_level()])) {
            for (std::uint32_t i = epoc::cap::BATTERY_LEVEL_MIN; i <= epoc::cap::BATTERY_LEVEL_MAX; i++) {
                if (ImGui::Selectable(BATTERY_LEVEL_STRS[i])) {
                    eik->get_pane_maintainer()->set_battery_level(i);
                }
            }

            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();

        ImGui::SameLine(col6 * 3 + 10);
        ImGui::Text("%s", sig_level_str.c_str());

        ImGui::SameLine(col6 * 4 + 10);
        ImGui::PushItemWidth(col6 * 2 - 10);

        if (ImGui::BeginCombo("##SignalLevel", BATTERY_LEVEL_STRS[eik->get_pane_maintainer()->get_signal_level()])) {
            for (std::uint32_t i = epoc::cap::BATTERY_LEVEL_MIN; i <= epoc::cap::BATTERY_LEVEL_MAX; i++) {
                if (ImGui::Selectable(BATTERY_LEVEL_STRS[i])) {
                    eik->get_pane_maintainer()->set_signal_level(i);
                }
            }

            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
    }

    void imgui_debugger::show_preferences() {
        const std::string pref_title = common::get_localised_string(localised_strings, "file_menu_prefs_item_name");
        ImGui::Begin(pref_title.c_str(), &should_show_preferences);

        using show_func = std::function<void(imgui_debugger *)>;
        std::vector<std::pair<std::string, show_func>> all_prefs = {
            { common::get_localised_string(localised_strings, "pref_general_title"), &imgui_debugger::show_pref_general },
            { common::get_localised_string(localised_strings, "pref_system_title"), &imgui_debugger::show_pref_system },
            { common::get_localised_string(localised_strings, "pref_control_title"), &imgui_debugger::show_pref_control },
            { common::get_localised_string(localised_strings, "pref_personalize_title"), &imgui_debugger::show_pref_personalisation },
            { "HAL", &imgui_debugger::show_pref_hal }
        };

        for (std::size_t i = 0; i < all_prefs.size(); i++) {
            if (ImGui::Button(all_prefs[i].first.c_str())) {
                cur_pref_tab = i;
            }

            ImGui::SameLine(0, 1);
        }

        ImGui::NewLine();

        ImGui::BeginChild("##PrefTab");
        all_prefs[cur_pref_tab].second(this);

        if (should_notify_reset_for_big_change) {
            ImGui::NewLine();

            const std::string restart_need_msg = common::get_localised_string(localised_strings, "pref_restart_needed_for_dvcchange_msg");
            ImGui::TextColored(RED_COLOR, restart_need_msg.c_str());
        }
        
        ImGui::NewLine();
        ImGui::NewLine();

        ImGui::SameLine(std::max(0.0f, ImGui::GetWindowSize().x / 2 - 30.0f));
        ImGui::PushItemWidth(30);

        const std::string save_str = common::get_localised_string(localised_strings, "save");
        if (ImGui::Button(save_str.c_str())) {
            device_manager *dvc_mngr = sys->get_device_manager();

            // Save devices.
            conf->serialize();
            dvc_mngr->save_devices();
        }

        ImGui::PopItemWidth();
        ImGui::EndChild();

        ImGui::End();
    }
}