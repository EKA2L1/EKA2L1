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
#include <debugger/logger.h>
#include <debugger/renderer/renderer.h>

#include <system/devices.h>
#include <system/epoc.h>
#include <package/manager.h>

#include <utils/locale.h>
#include <kernel/kernel.h>

#include <services/applist/applist.h>
#include <services/ui/cap/oom_app.h>
#include <services/window/window.h>

#include <utils/system.h>

#include <drivers/graphics/emu_window.h> // For scancode
#include <drivers/itc.h>

#include <config/config.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/language.h>
#include <common/platform.h>

#include <yaml-cpp/yaml.h>
#include <debugger/imgui_consts.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace eka2l1 {
    static void language_property_change_handler(void *userdata, service::property *prop) {
        imgui_debugger *debugger = reinterpret_cast<imgui_debugger *>(userdata);
        config::state *conf = debugger->get_config();

        conf->language = static_cast<int>(debugger->get_language_from_property(prop));
        conf->serialize();

        prop->add_data_change_callback(userdata, language_property_change_handler);
    }

    imgui_debugger::imgui_debugger(eka2l1::system *sys, imgui_logger *logger)
        : sys(sys)
        , conf(sys->get_config())
        , logger(logger)
        , should_stop(false)
        , should_pause(false)
        , should_show_threads(false)
        , should_show_mutexs(false)
        , should_show_chunks(false)
        , should_show_window_tree(false)
        , should_show_disassembler(false)
        , should_show_logger(true)
        , should_show_preferences(false)
        , should_package_manager(false)
        , should_package_manager_display_file_list(false)
        , should_package_manager_remove(false)
        , should_package_manager_display_installer_text(false)
        , should_package_manager_display_one_button_only(false)
        , should_package_manager_display_language_choose(false)
        , should_install_package(false)
        , should_show_app_launch(false)
        , should_app_launch_use_new_style(true)
        , should_still_focus_on_keyboard(true)
        , should_show_install_device_wizard(false)
        , should_show_about(false)
        , should_show_empty_device_warn(false)
        , should_notify_reset_for_big_change(false)
        , should_disable_validate_drive(false)
        , should_warn_touch_disabled(false)
        , should_show_sd_card_mount(false)
        , request_key(false)
        , key_set(false)
        , selected_package_index(0xFFFFFFFF)
        , debug_thread_id(0)
        , selected_callback(nullptr)
        , selected_callback_data(nullptr)
        , phony_icon(0)
        , icon_placeholder_icon(0)
        , active_screen(0)
        , alserv(nullptr)
        , winserv(nullptr)
        , oom(nullptr)
        , should_show_menu_fullscreen(false)
        , sd_card_mount_choosen(false) 
        , back_from_fullscreen(false)
        , last_scale(1.0f)
        , last_cursor(ImGuiMouseCursor_Arrow)
        , font_to_use(nullptr)
        , active_app_config(nullptr) {
        if (conf->emulator_language == -1) {
            conf->emulator_language = static_cast<int>(language::en);
        }

        should_app_launch_use_new_style = conf->ui_new_style;

        localised_strings = common::get_localised_string_table("resources", "strings.xml", static_cast<language>(conf->emulator_language));
        std::fill(device_wizard_state.should_continue_temps, device_wizard_state.should_continue_temps + 2,
            false);

        // Check if no device is installed
        device_manager *dvc_mngr = sys->get_device_manager();
        if (dvc_mngr->total() == 0) {
            should_show_empty_device_warn = true;
        }

        // Setup hook
        manager::packages *pkg_mngr = sys->get_packages();

        pkg_mngr->show_text = [&](const char *text_buf, const bool one_button) -> bool {
            installer_text = text_buf;
            should_package_manager_display_installer_text = true;
            should_package_manager_display_one_button_only = one_button;

            std::unique_lock<std::mutex> ul(installer_mut);
            installer_cond.wait(ul);

            // Normally debug thread won't touch this
            should_package_manager_display_installer_text = false;

            return installer_text_result;
        };

        pkg_mngr->choose_lang = [&](const int *lang, const int count) -> int {
            installer_langs = lang;
            installer_lang_size = count;

            should_package_manager_display_language_choose = true;

            std::unique_lock<std::mutex> ul(installer_mut);
            installer_cond.wait(ul);

            // Normally debug thread won't touch this
            should_package_manager_display_language_choose = false;

            return installer_lang_choose_result;
        };

        install_thread = std::make_unique<std::thread>([&]() {
            while (!install_thread_should_stop) {
                while (auto result = install_list.pop()) {
                    if (!install_thread_should_stop) {
                        get_sys()->install_package(common::utf8_to_ucs2(result.value()), drive_e);
                    }
                }

                std::unique_lock<std::mutex> ul(install_thread_mut);
                install_thread_cond.wait(ul);
            }
        });

        // Load credits
        YAML::Node the_node;

        try {
            static constexpr const char *CREDIT_PATH = "resources//credits.yml";
            the_node = YAML::LoadFile(CREDIT_PATH);
        } catch (std::exception &e) {
            LOG_ERROR("Unable to load credits file. Error description: {}", e.what());
        }

        // Respect each section. If one section can't load. That does not mean the further will not load too.
        try {
            for (const auto node: the_node["MainDevs"]) {
                main_dev_strings.push_back(node.as<std::string>());
            }
        } catch (std::exception &e) {
            LOG_ERROR("Unable to load main developers credits. Error description: {}", e.what());
        }

        try {
            for (const auto node: the_node["Contributors"]) {
                contributors_strings.push_back(node.as<std::string>());
            }
        } catch (std::exception &e) {
            LOG_ERROR("Unable to load contributors credits. Error description: {}", e.what());
        }

        try {
            for (const auto node: the_node["Honors"]) {
                honors_strings.push_back(node.as<std::string>());
            }
        } catch (std::exception &e) {
            LOG_ERROR("Unable to load honors credits. Error description: {}", e.what());
        }

        try {
            for (const auto node: the_node["TranslatorsDesktop"]) {
                translators_strings.push_back(node.as<std::string>());
            }
        } catch (std::exception &e) {
            LOG_ERROR("Unable to load translators credits. Error description: {}", e.what());
        }

        kernel_system *kern = sys->get_kernel_system();

        if (kern) {
            alserv = reinterpret_cast<eka2l1::applist_server *>(kern->get_by_name<service::server>(get_app_list_server_name_by_epocver(
                kern->get_epoc_version())));
            winserv = reinterpret_cast<eka2l1::window_server *>(kern->get_by_name<service::server>(eka2l1::get_winserv_name_by_epocver(
                kern->get_epoc_version())));
            oom = reinterpret_cast<eka2l1::oom_ui_app_server *>(kern->get_by_name<service::server>("101fdfae_10207218_AppServer"));

            property_ptr lang_prop = kern->get_prop(epoc::SYS_CATEGORY, epoc::LOCALE_LANG_KEY);

            if (lang_prop)
                lang_prop->add_data_change_callback(this, language_property_change_handler);
        }

        key_binder_state.target_key = {
            KEY_UP,
            KEY_DOWN,
            KEY_LEFT,
            KEY_RIGHT,
            KEY_ENTER,
            KEY_F1,
            KEY_F2,
            KEY_NUM0,
            KEY_NUM1,
            KEY_NUM2,
            KEY_NUM3,
            KEY_NUM4,
            KEY_NUM5,
            KEY_NUM6,
            KEY_NUM7,
            KEY_NUM8,
            KEY_NUM9,
            KEY_SLASH,
            KEY_STAR,
            KEY_BACKSPACE
        };
        key_binder_state.target_key_name = {
            "KEY_UP",
            "KEY_DOWN",
            "KEY_LEFT",
            "KEY_RIGHT",
            "KEY_ENTER",
            "KEY_F1",
            "KEY_F2",
            "KEY_NUM0",
            "KEY_NUM1",
            "KEY_NUM2",
            "KEY_NUM3",
            "KEY_NUM4",
            "KEY_NUM5",
            "KEY_NUM6",
            "KEY_NUM7",
            "KEY_NUM8",
            "KEY_NUM9",
            "KEY_SLASH",
            "KEY_STAR",
            "KEY_BACKSPACE"
        };
        key_binder_state.key_bind_name = {
            { KEY_UP, "KEY_UP" },
            { KEY_DOWN, "KEY_DOWN" },
            { KEY_LEFT, "KEY_LEFT" },
            { KEY_RIGHT, "KEY_RIGHT" },
            { KEY_ENTER, "KEY_ENTER" },
            { KEY_F1, "KEY_F1" },
            { KEY_F2, "KEY_F2" },
            { KEY_NUM0, "KEY_NUM0" },
            { KEY_NUM1, "KEY_NUM1" },
            { KEY_NUM2, "KEY_NUM2" },
            { KEY_NUM3, "KEY_NUM3" },
            { KEY_NUM4, "KEY_NUM4" },
            { KEY_NUM5, "KEY_NUM5" },
            { KEY_NUM6, "KEY_NUM6" },
            { KEY_NUM7, "KEY_NUM7" },
            { KEY_NUM8, "KEY_NUM8" },
            { KEY_NUM9, "KEY_NUM9" },
            { KEY_SLASH, "KEY_SLASH" },
            { KEY_STAR, "KEY_STAR" },
            { KEY_BACKSPACE, "KEY_BACKSPACE" }
        };

        key_binder_state.need_key = std::vector<bool>(key_binder_state.BIND_NUM, false);

        if (winserv) {
            for (auto &kb : conf->keybinds) {
                bool is_mouse = (kb.source.type == config::KEYBIND_TYPE_MOUSE);

                if (is_mouse) {
                    should_warn_touch_disabled = !conf->stop_warn_touch_disabled;
                }

                if ((kb.source.type == config::KEYBIND_TYPE_KEY) || is_mouse) {
                    const std::uint32_t bind_keycode = (is_mouse ? epoc::KEYBIND_TYPE_MOUSE_CODE_BASE : 0) + kb.source.data.keycode;

                    winserv->input_mapping.key_input_map[bind_keycode] = kb.target;
                    key_binder_state.key_bind_name[kb.target] = (is_mouse ? "M" : "") + std::to_string(kb.source.data.keycode);
                } else if (kb.source.type == config::KEYBIND_TYPE_CONTROLLER) {
                    winserv->input_mapping.button_input_map[std::make_pair(kb.source.data.button.controller_id, kb.source.data.button.button_id)] = kb.target;
                    key_binder_state.key_bind_name[kb.target] = std::to_string(kb.source.data.button.controller_id) + ":" + std::to_string(kb.source.data.button.button_id);
                }
            }
        }
    }

    imgui_debugger::~imgui_debugger() {
        install_thread_should_stop = true;
        install_thread_cond.notify_one();

        if (install_thread) {
            install_thread->join();
        }

        if (device_wizard_state.install_thread) {
            device_wizard_state.install_thread->join();
        }
    }

    config::state *imgui_debugger::get_config() {
        return conf;
    }
    
    void imgui_debugger::show_menu() {
        int num_pops = 0;

        if (renderer->is_fullscreen()) {    
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.00001f);
            num_pops++;
        }

        if (ImGui::BeginMainMenuBar()) {
            conf->menu_height = ImGui::GetWindowSize().y;

            if (should_show_menu_fullscreen) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
                num_pops++;
            }

            const std::string file_menu_item_name = common::get_localised_string(localised_strings, "file_menu_name");
            
            if (ImGui::BeginMenu(file_menu_item_name.c_str())) {
                const std::string logger_item_name = common::get_localised_string(localised_strings, "file_menu_logger_item_name");
                const std::string launch_app_item_name = common::get_localised_string(localised_strings, "file_menu_launch_apps_item_name");
                const std::string package_item_name = common::get_localised_string(localised_strings, "file_menu_packages_item_name");

                ImGui::MenuItem(logger_item_name.c_str(), "CTRL+SHIFT+L", &should_show_logger);

                bool last_show_launch = should_show_app_launch;
                ImGui::MenuItem(launch_app_item_name.c_str(), "CTRL+R", &should_show_app_launch);

                if ((last_show_launch != should_show_app_launch) && last_show_launch == false) {
                    should_still_focus_on_keyboard = true;
                }

                device_manager *dvc_mngr = sys->get_device_manager();
                if (!dvc_mngr->get_current()) {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

                    ImGui::MenuItem(package_item_name.c_str());

                    if (ImGui::IsItemHovered()) {
                        ImGui::PopStyleVar();

                        const std::string unavail_tt = common::get_localised_string(localised_strings, "file_menu_packages_item_unavail_msg");
                        ImGui::SetTooltip(unavail_tt.c_str());
                    } else {
                        ImGui::PopStyleVar();
                    }
                } else {
                    if (ImGui::BeginMenu(package_item_name.c_str())) {
                        const std::string package_install_item_name = common::get_localised_string(localised_strings,
                            "file_menu_package_submenu_install_item_name");

                        const std::string list_install_item_name = common::get_localised_string(localised_strings,
                            "file_menu_package_submenu_list_item_name");

                        ImGui::MenuItem(package_install_item_name.c_str(), nullptr, &should_install_package);
                        ImGui::MenuItem(list_install_item_name.c_str(), nullptr, &should_package_manager);
                        ImGui::EndMenu();
                    }
                }

                ImGui::Separator();

                const std::string install_device_item_name = common::get_localised_string(localised_strings,
                    "file_menu_install_device_item_name");

                const std::string mount_mmc_item_name = common::get_localised_string(localised_strings,
                    "file_menu_mount_sd_item_name");
                    
                ImGui::MenuItem(install_device_item_name.c_str(), nullptr, &should_show_install_device_wizard);
                ImGui::MenuItem(mount_mmc_item_name.c_str(), "CTRL+SHIFT+X", &should_show_sd_card_mount);

                ImGui::Separator();

                const std::string pref_item_name = common::get_localised_string(localised_strings, "file_menu_prefs_item_name");
                ImGui::MenuItem(pref_item_name.c_str(), nullptr, &should_show_preferences);

                ImGui::EndMenu();
            }

            const std::string debugger_menu_name = common::get_localised_string(localised_strings, "debugger_menu_name");

            if (ImGui::BeginMenu(debugger_menu_name.c_str())) {
                const std::string pause_item_name = common::get_localised_string(localised_strings, "debugger_menu_pause_item_name");
                const std::string stop_item_name = common::get_localised_string(localised_strings, "debugger_menu_stop_item_name");
    
                ImGui::MenuItem(pause_item_name.c_str(), "CTRL+P", &should_pause);
                ImGui::MenuItem(stop_item_name.c_str(), nullptr, &should_stop);

                ImGui::Separator();

                const std::string disassembler_item_name = common::get_localised_string(localised_strings,
                    "debugger_menu_disassembler_item_name");

                const std::string object_submenu_name = common::get_localised_string(localised_strings,
                    "debugger_menu_objects_item_name");

                ImGui::MenuItem(disassembler_item_name.c_str(), nullptr, &should_show_disassembler);

                if (ImGui::BeginMenu(object_submenu_name.c_str())) {
                    const std::string threads_item_name = common::get_localised_string(localised_strings,
                        "debugger_menu_objects_submenu_threads_item_name");

                    const std::string mutexes_item_name = common::get_localised_string(localised_strings,
                        "debugger_menu_objects_submenu_mutexes_item_name");

                    const std::string chunks_item_name = common::get_localised_string(localised_strings,
                        "debugger_menu_objects_submenu_chunks_item_name");
                        
                    ImGui::MenuItem(threads_item_name.c_str(), nullptr, &should_show_threads);
                    ImGui::MenuItem(mutexes_item_name.c_str(), nullptr, &should_show_mutexs);
                    ImGui::MenuItem(chunks_item_name.c_str(), nullptr, &should_show_chunks);
                    ImGui::EndMenu();
                }

                const std::string services_submenu_name = common::get_localised_string(localised_strings,
                    "debugger_menu_services_item_name");

                if (ImGui::BeginMenu(services_submenu_name.c_str())) {
                    const std::string window_tree_item_name = common::get_localised_string(localised_strings,
                        "debugger_menu_services_submenu_window_tree_item_name");

                    ImGui::MenuItem(window_tree_item_name.c_str(), nullptr, &should_show_window_tree);
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }
            
            const std::string view_menu_name = common::get_localised_string(localised_strings, "view_menu_name");

            if (ImGui::BeginMenu(view_menu_name.c_str())) {
                bool fullscreen = renderer->is_fullscreen();
                bool last_full_status = fullscreen;

                const std::string fullscreen_item_name = common::get_localised_string(localised_strings,
                    "view_menu_fullscreen_item_name");
                
                ImGui::MenuItem(fullscreen_item_name.c_str(), nullptr, &fullscreen);
                renderer->set_fullscreen(fullscreen);

                if ((fullscreen == false) && (last_full_status != fullscreen)) {
                    back_from_fullscreen = true;
                }

                ImGui::EndMenu();
            }

            const std::string help_menu_name = common::get_localised_string(localised_strings, "help_menu_name");

            if (ImGui::BeginMenu(help_menu_name.c_str())) {
                const std::string about_item_name = common::get_localised_string(localised_strings,
                    "help_menu_about_item_name");

                ImGui::MenuItem(about_item_name.c_str(), nullptr, &should_show_about);
                ImGui::EndMenu();
            }

            if (renderer->is_fullscreen()) {
                should_show_menu_fullscreen = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) 
                    || ImGui::IsWindowHovered(ImGuiHoveredFlags_RectOnly);
            } else {
                should_show_menu_fullscreen = false;
            }

            ImGui::EndMainMenuBar();
        }

        if (num_pops) {
            ImGui::PopStyleVar(num_pops);
        }
    }

    void imgui_debugger::handle_shortcuts() {
        ImGuiIO &io = ImGui::GetIO();

        if (io.KeyCtrl) {
            if (io.KeyShift) {
                if (io.KeysDown[KEY_L]) { // Logger
                    should_show_logger = !should_show_logger;
                    io.KeysDown[KEY_L] = false;
                }

                if (io.KeysDown[KEY_X]) {
                    should_show_sd_card_mount = !should_show_sd_card_mount;
                    io.KeysDown[KEY_X] = false;
                }

                io.KeyShift = false;
            }

            if (io.KeysDown[KEY_R]) {
                should_show_app_launch = !should_show_app_launch;
                io.KeysDown[KEY_R] = false;
            }

            if (io.KeysDown[KEY_F11]) {
                renderer->set_fullscreen(!renderer->is_fullscreen());
                io.KeysDown[KEY_F11] = false;
            }

            io.KeyCtrl = false;
        }
    }

    void imgui_debugger::show_debugger(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width, std::uint32_t fb_height) {
        bool pushed = false;
        
        if (font_to_use) {
            ImGui::PushFont(font_to_use);
            pushed = true;
        }

        auto cleanup_show = [pushed]() {
            if (pushed) {
                ImGui::PopFont();
            }
        };

        show_menu();
        handle_shortcuts();
        show_screens();
        show_errors();

        if (should_show_empty_device_warn) {
            show_empty_device_warn();
            cleanup_show();

            return;
        }

        if (should_warn_touch_disabled) {
            show_touchscreen_disabled_warn();
            cleanup_show();

            return;
        }

        if (should_package_manager) {
            show_package_manager();
        }

        if (should_show_threads) {
            show_threads();
        }

        if (should_show_mutexs) {
            show_mutexs();
        }

        if (should_show_chunks) {
            show_chunks();
        }

        if (should_show_window_tree) {
            show_windows_tree();
        }

        if (should_show_disassembler) {
            show_disassembler();
        }

        if (should_show_preferences) {
            show_preferences();
        }

        if (should_package_manager_display_installer_text) {
            show_installer_text_popup();
        }

        if (should_package_manager_display_language_choose) {
            show_installer_choose_lang_popup();
        }

        if (should_install_package) {
            should_install_package = false;
            do_install_package();
        }

        if (should_show_app_launch) {
            show_app_launch();
        }

        if (should_show_install_device_wizard) {
            show_install_device();
        }

        if (should_show_sd_card_mount) {
            show_mount_sd_card();
        }

        if (should_show_about) {
            show_about();
        }

        on_pause_toogle(should_pause);

        if (should_show_logger) {
            logger->draw("Logger", &should_show_logger);
        }

        cleanup_show();
    }

    void imgui_debugger::wait_for_debugger() {
        should_pause = true;

        std::unique_lock ulock(debug_lock);
        debug_cv.wait(ulock);
    }

    void imgui_debugger::notify_clients() {
        debug_cv.notify_all();
    }

    void imgui_debugger::set_logger_visbility(const bool show) {
        should_show_logger = show;
    }
}
