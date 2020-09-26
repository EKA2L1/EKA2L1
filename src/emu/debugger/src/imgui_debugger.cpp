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

#include <cpu/arm_utils.h>

#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <debugger/renderer/common.h>
#include <debugger/renderer/renderer.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <disasm/disasm.h>
#include <epoc/epoc.h>

#include <utils/apacmd.h>
#include <utils/locale.h>

#include <kernel/kernel.h>
#include <kernel/libmanager.h>
#include <kernel/thread.h>

#include <services/applist/applist.h>
#include <services/ui/cap/eiksrv.h>
#include <services/ui/cap/oom_app.h>
#include <services/window/classes/winbase.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/common.h>
#include <services/window/window.h>
#include <utils/system.h>

#include <drivers/graphics/emu_window.h> // For scancode

#include <config/config.h>
#include <manager/device_manager.h>
#include <manager/manager.h>
#include <manager/installation/raw_dump.h>
#include <manager/installation/rpkg.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/language.h>
#include <common/path.h>
#include <common/platform.h>

#include <chrono>
#include <mutex>
#include <thread>

#define RGBA_TO_FLOAT(r, g, b, a) ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f)

const ImVec4 GUI_COLOR_TEXT_MENUBAR = RGBA_TO_FLOAT(242.0f, 150.0f, 58.0f, 255.0f);
const ImVec4 GUI_COLOR_TEXT_MENUBAR_OPTIONS = RGBA_TO_FLOAT(242.0f, 150.0f, 58.0f, 255.0f);
const ImVec4 GUI_COLOR_TEXT_TITLE = RGBA_TO_FLOAT(247.0f, 198.0f, 51.0f, 255.0f);
const ImVec4 GUI_COLOR_TEXT = RGBA_TO_FLOAT(255, 255, 255, 255);
const ImVec4 GUI_COLOR_TEXT_SELECTED = RGBA_TO_FLOAT(125.0f, 251.0f, 143.0f, 255.0f);

static const ImVec4 RED_COLOR = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

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
        , active_screen(0)
        , alserv(nullptr)
        , winserv(nullptr)
        , oom(nullptr)
        , should_show_menu_fullscreen(false)
        , sd_card_mount_choosen(false) 
        , back_from_fullscreen(false)
        , last_scale(1.0f)
        , last_cursor(ImGuiMouseCursor_Arrow)
        , font_to_use(nullptr) {
        if (conf->emulator_language == -1) {
            conf->emulator_language = static_cast<int>(language::en);
        }

        localised_strings = common::get_localised_string_table("resources", "strings.xml", static_cast<language>(conf->emulator_language));
        std::fill(device_wizard_state.should_continue_temps, device_wizard_state.should_continue_temps + 2,
            false);

        manager_system *mngr = sys->get_manager_system();

        // Check if no device is installed
        manager::device_manager *dvc_mngr = mngr->get_device_manager();
        if (dvc_mngr->total() == 0) {
            should_show_empty_device_warn = true;
        }

        // Setup hook
        manager::package_manager *pkg_mngr = mngr->get_package_manager();

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
            KEY_STAR
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
            "KEY_STAR"
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
            { KEY_STAR, "KEY_STAR" }
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

    const char *thread_state_to_string(eka2l1::kernel::thread_state state) {
        switch (state) {
        case kernel::thread_state::stop:
            return "stop";
        case kernel::thread_state::create:
            return "unschedule";
        case kernel::thread_state::wait:
            return "wating";
        case kernel::thread_state::wait_fast_sema:
            return "waiting (fast semaphore)";
        case kernel::thread_state::wait_mutex:
            return "waiting (mutex)";
        case kernel::thread_state::wait_mutex_suspend:
            return "suspend (mutex)";
        case kernel::thread_state::wait_fast_sema_suspend:
            return "suspend (fast sema)";
        case kernel::thread_state::hold_mutex_pending:
            return "holded (mutex)";
        case kernel::thread_state::ready:
            return "ready";
        case kernel::thread_state::run:
            return "running";
        default:
            break;
        }

        return "unknown";
    }

    void imgui_debugger::show_threads() {
        if (ImGui::Begin("Threads", &should_show_threads)) {
            // Only the stack are created by the OS
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s    %-32s", "ID",
                "Thread name", "State");

            const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock_);

            for (const auto &thr_obj : sys->get_kernel_system()->threads_) {
                kernel::thread *thr = reinterpret_cast<kernel::thread *>(thr_obj.get());
                chunk_ptr chnk = thr->get_stack_chunk();

                ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X    %-32s    %-32s", thr->unique_id(),
                    thr->name().c_str(), thread_state_to_string(thr->current_state()));
            }
        }

        ImGui::End();
    }

    void imgui_debugger::show_mutexs() {
        if (ImGui::Begin("Mutexs", &should_show_mutexs)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s", "ID",
                "Mutex name");

            const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock_);

            for (const auto &mutex : sys->get_kernel_system()->mutexes_) {
                ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X    %-32s", mutex->unique_id(),
                    mutex->name().c_str());
            }
        }

        ImGui::End();
    }

    void imgui_debugger::show_chunks() {
        if (ImGui::Begin("Chunks", &should_show_chunks)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-24s         %-8s        %-8s      %-32s", "ID",
                "Chunk name", "Committed", "Max", "Creator process");

            const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock_);

            for (const auto &chnk_obj : sys->get_kernel_system()->chunks_) {
                kernel::chunk *chnk = reinterpret_cast<kernel::chunk *>(chnk_obj.get());
                std::string process_name = chnk->get_own_process() ? chnk->get_own_process()->name() : "Unknown";

                ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X    %-32s      0x%08lX        0x%08lX      %-32s",
                    chnk->unique_id(), chnk->name().c_str(), chnk->committed(), chnk->max_size(), process_name.c_str());
            }
        }

        ImGui::End();
    }

    void imgui_debugger::show_timers() {
    }

    void imgui_debugger::show_disassembler() {
        if (ImGui::Begin("Disassembler", &should_show_disassembler)) {
            thread_ptr debug_thread = nullptr;
            kernel_system *kern = sys->get_kernel_system();

            if (!debug_thread_id) {
                const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock_);

                if (kern->threads_.size() == 0) {
                    ImGui::End();
                    return;
                }

                debug_thread = reinterpret_cast<kernel::thread *>(kern->threads_.begin()->get());
                debug_thread_id = debug_thread->unique_id();
            } else {
                debug_thread = kern->get_by_id<kernel::thread>(debug_thread_id);
            }

            std::string thr_name = debug_thread->name();

            if (ImGui::BeginCombo("Thread", debug_thread ? thr_name.c_str() : "Thread")) {
                for (std::size_t i = 0; i < kern->threads_.size(); i++) {
                    std::string cr_thrname = kern->threads_[i]->name() + " (ID " + common::to_string(kern->threads_[i]->unique_id()) + ")";

                    if (ImGui::Selectable(cr_thrname.c_str(), kern->threads_[i]->unique_id() == debug_thread_id)) {
                        debug_thread_id = kern->threads_[i]->unique_id();
                        debug_thread = reinterpret_cast<kernel::thread *>(kern->threads_[i].get());
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::NewLine();

            if (debug_thread) {
                arm::core::thread_context &ctx = debug_thread->get_thread_context();

                for (std::uint32_t pc = ctx.pc - 12, i = 0; i < 12; i++) {
                    void *codeptr = debug_thread->owning_process()->get_ptr_on_addr_space(pc);

                    if (!codeptr) {
                        break;
                    }

                    const std::string dis = sys->get_disasm()->disassemble(reinterpret_cast<const std::uint8_t *>(codeptr), ctx.cpsr & 0x20 ? 2 : 4,
                        pc, ctx.cpsr & 0x20);

                    if (dis.find("svc") == std::string::npos) {
                        ImGui::Text("0x%08x: %-10u    %s", pc, *reinterpret_cast<std::uint32_t *>(codeptr), dis.c_str());
                    } else {
                        const std::uint32_t svc_num = std::stoul(dis.substr(5), nullptr, 16);
                        const std::string svc_call_name = sys->get_lib_manager()->svc_funcs_.find(svc_num) != sys->get_lib_manager()->svc_funcs_.end() ? sys->get_lib_manager()->svc_funcs_[svc_num].name : "Unknown";

                        ImGui::Text("0x%08x: %-10u    %s            ; %s", pc, *reinterpret_cast<std::uint32_t *>(codeptr), dis.c_str(), svc_call_name.c_str());
                    }

                    pc += (ctx.cpsr & 0x20) ? 2 : 4;
                }

                // In the end, dump the thread context

                ImGui::NewLine();
                ImGui::Text("PC: 0x%08X         SP: 0x%08X         LR: 0x%08X", ctx.pc, ctx.sp, ctx.lr);
                ImGui::Text("r0: 0x%08X         r1: 0x%08X         r2: 0x%08X", ctx.cpu_registers[0], ctx.cpu_registers[1], ctx.cpu_registers[2]);
                ImGui::Text("r3: 0x%08X         r4: 0x%08X         r5: 0x%08X", ctx.cpu_registers[3], ctx.cpu_registers[4], ctx.cpu_registers[5]);
                ImGui::Text("r6: 0x%08X         r7: 0x%08X         r8: 0x%08X", ctx.cpu_registers[6], ctx.cpu_registers[7], ctx.cpu_registers[8]);
                ImGui::Text("r9: 0x%08X         r10: 0x%08X        r11: 0x%08X", ctx.cpu_registers[9], ctx.cpu_registers[10], ctx.cpu_registers[11]);
                ImGui::Text("r12: 0x%08X        r13: 0x%08X        r14: 0x%08X", ctx.cpu_registers[12], ctx.cpu_registers[13], ctx.cpu_registers[14]);
                ImGui::Text("r15: 0x%08X        CPSR: 0x%08X", ctx.cpu_registers[15], ctx.cpsr);
            }
        }

        ImGui::End();
    }

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
        const std::string font_str = common::get_localised_string(localised_strings, "pref_personalize_font_string");

        ImGui::Text("%s ", transparency_str.c_str());
        ImGui::SameLine();
        ImGui::SliderInt("##BackgroundTransparency", &conf->bkg_transparency, 0, 255);

        ImGui::Text("%s     ", ui_scale_str.c_str());
        ImGui::SameLine();
        const bool ret = ImGui::InputFloat("##UIScale", &conf->ui_scale, 0.1f);
        if (ret && conf->ui_scale <= 1e-6) {
            conf->ui_scale = 0.5;
        }

        ImGui::NewLine();
        ImGui::Text("%s", font_str.c_str());
        ImGui::Separator();

        ImGui::Text("%s         ", path_str.c_str());
        ImGui::SameLine();
        ImGui::InputText("##FontPath", conf->font_path.data(), conf->font_path.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();

        const std::string replace_str = common::get_localised_string(localised_strings, "replace");
        if (ImGui::Button(replace_str.c_str())) {
            on_pause_toogle(true);

            drivers::open_native_dialog(sys->get_graphics_driver(), "ttf", [&](const char *result) {
                conf->font_path = result;
                conf->serialize();
            });

            should_pause = false;
            on_pause_toogle(false);
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
            manager::device_manager *dvc_mngr = sys->get_manager_system()->get_device_manager();
            dvc_mngr->load_devices();

            should_notify_reset_for_big_change = true;
        }

        const std::string deb_title = common::get_localised_string(localised_strings, "pref_general_debugging_string");

        ImGui::Text("%s", deb_title.c_str());
        ImGui::Separator();

        const float col2 = ImGui::GetWindowSize().x / 2;

        const std::string cpu_read_str = common::get_localised_string(localised_strings, "pref_general_debugging_cpu_read_checkbox_title");
        const std::string cpu_write_str = common::get_localised_string(localised_strings, "pref_general_debugging_cpu_write_checkbox_title");
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

        const std::string utils_sect_title = common::get_localised_string(localised_strings, "pref_general_utilities_string");
        const std::string utils_hide_mouse_str = common::get_localised_string(localised_strings, "pref_general_utilities_hide_cursor_in_screen_space_checkbox_title");
           
        ImGui::NewLine();
        ImGui::Text("%s", utils_sect_title.c_str());
        ImGui::Separator();

        ImGui::Checkbox(utils_hide_mouse_str.c_str(), &conf->hide_mouse_in_screen_space);
        if (ImGui::IsItemHovered()) {
            const std::string mouse_hide_tt = common::get_localised_string(localised_strings, "pref_general_utilities_hide_cursor_in_screen_space_tooltip_msg");
            ImGui::SetTooltip("%s", mouse_hide_tt.c_str());
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

        manager::device_manager *mngr = sys->get_manager_system()->get_device_manager();
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
                    }

                    conf->device = static_cast<int>(i);
                    conf->serialize();

                    mngr->lock.unlock();
                    mngr->set_current(static_cast<std::uint8_t>(i));
                    mngr->lock.lock();

                    should_notify_reset_for_big_change = true;
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

            static const char *TIME_UNIT_NAME = "US";

            const std::string time_delay_str = common::get_localised_string(localised_strings, "pref_system_time_delay_option_name");

            ImGui::Text("%s", time_delay_str.c_str());
            ImGui::SameLine(col2);
            ImGui::PushItemWidth(col2 - ImGui::CalcTextSize(TIME_UNIT_NAME).x - 15);

            int current_delay = conf->time_getter_sleep_us;
            
            static constexpr int MIN_TIME_DELAY = 0;
            static constexpr int MAX_TIME_DELAY = 500;

            if (ImGui::SliderInt(TIME_UNIT_NAME, &current_delay, MIN_TIME_DELAY, MAX_TIME_DELAY)) {
                if (current_delay != conf->time_getter_sleep_us) {
                    dvc.time_delay_us = static_cast<std::uint16_t>(current_delay);
                    conf->time_getter_sleep_us = static_cast<std::uint16_t>(current_delay);
                }
            }

            ImGui::PopItemWidth();

            kernel_system *kern = sys->get_kernel_system();
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

            if (ImGui::IsItemHovered()) {
                const std::string tooltip = common::get_localised_string(localised_strings, "pref_system_validate_device_tooltip");
                std::string tooltip_disabled = "";

                if (should_disable_validate_drive) {
                    tooltip_disabled = common::get_localised_string(localised_strings, "pref_system_validate_device_disabled_tooltip");
                }

                ImGui::SetTooltip("%s\n%s", tooltip.c_str(), tooltip_disabled.c_str());
            }
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
            oom->init(sys->get_kernel_system());
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
            manager::device_manager *dvc_mngr = sys->get_manager_system()->get_device_manager();

            // Save devices.
            conf->serialize();
            dvc_mngr->save_devices();
        }

        ImGui::PopItemWidth();
        ImGui::EndChild();

        ImGui::End();
    }

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
        manager::package_manager *manager = sys->get_manager_system()->get_package_manager();

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

        clipper.End();

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
            const std::lock_guard<std::mutex> guard(manager->lockdown);
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
                        manager->uninstall_package(pkg->id);
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
                        manager::device_manager *manager = sys->get_manager_system()->get_device_manager();

                        device_wizard_state.install_thread = std::make_unique<std::thread>([](
                                                                                               manager::device_manager *mngr, device_wizard *wizard, config::state *conf) {
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
                                LOG_ERROR("Unable to copy ROM to target ROM directory!");
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

    void imgui_debugger::show_mount_sd_card() {
        if (!sd_card_mount_choosen) {
            if (!drivers::open_native_dialog(sys->get_graphics_driver(), "", [&](const char *result) {
                sys->mount(drive_e, drive_media::physical, result, io_attrib_removeable);
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

            ImVec2 mount_success_window_size = ImVec2(400.0f, 100.0f);
            mount_success_window_size.x = ImGui::CalcTextSize(card_mount_succ_msg.c_str()).x + 20.0f;

            ImGui::SetNextWindowSize(mount_success_window_size);

            if (ImGui::BeginPopupModal(card_mount_succ_title.c_str())) {
                ImGui::Text("%s", card_mount_succ_msg.c_str());

                ImGui::NewLine();
                ImGui::NewLine();

                ImGui::SameLine((mount_success_window_size.x - 30.0f) / 2);

                ImGui::PushItemWidth(30.0f);
                
                if (ImGui::Button(ok_str.c_str())) {
                    should_show_sd_card_mount = false;
                    sd_card_mount_choosen = false;
                }

                ImGui::PopItemWidth();

                ImGui::EndPopup();
            }
        }
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

                manager::device_manager *dvc_mngr = sys->get_manager_system()->get_device_manager();
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
                ImGui::MenuItem(mount_mmc_item_name.c_str(), nullptr, &should_show_sd_card_mount);

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

    void imgui_debugger::show_app_launch() {
        static ImGuiTextFilter app_search_box;

        const std::string app_launcher_title = common::get_localised_string(localised_strings, "app_launcher_title");
        ImGui::Begin(app_launcher_title.c_str(), &should_show_app_launch);

        if (alserv) {
            std::vector<apa_app_registry> &registerations = alserv->get_registerations();
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

            ImGui::BeginChild("##AppListScroll");
            {
                ImGui::Columns(2);

                ImGui::SetColumnWidth(0, uid_col_size);

                ImGuiListClipper clipper;

                if (app_search_box.Filters.empty())
                    clipper.Begin(static_cast<int>(registerations.size()), ImGui::GetTextLineHeight());

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

                if (app_search_box.Filters.empty())
                    clipper.End();

                ImGui::EndChild();
            }
            ImGui::Columns(1);
        } else {
            const std::string unavail_msg = common::get_localised_string(localised_strings, "app_launcher_unavail_msg");
            ImGui::Text("%s", unavail_msg.c_str());
        }

        ImGui::End();
    }

    void imgui_debugger::queue_error(const std::string &error) {
        const std::lock_guard<std::mutex> guard(errors_mut);
        error_queue.push(error);
    }

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

    static void get_nice_scale_by_integer(ImVec2 out_size, ImVec2 source_size, eka2l1::vec2 &scale) {
        scale.x = scale.y = 1;

        while ((source_size.x * scale.x < out_size.x) && (source_size.y * scale.y < out_size.y)) {
            scale.x++;
            scale.y++;
        }

        if (scale.x > 1)
            scale.x--;
        if (scale.y > 1)
            scale.y--;
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

            ImVec2 fullscreen_region = ImVec2(io.DisplaySize.x, io.DisplaySize.y - 60.0f);

            const float fullscreen_start_x = 0.0f;
            const float fullscreen_start_y = 30.0f;

            float ratioed = rotated_size.x / rotated_size.y;

            if (fullscreen_now) {
                ImGui::SetNextWindowSize(fullscreen_region);
                ImGui::SetNextWindowPos(ImVec2(fullscreen_start_x, fullscreen_start_y));
            } else {
                ImGui::SetNextWindowSize(rotated_size, ImGuiCond_Once);

                if (back_from_fullscreen) {
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
            get_nice_scale_by_integer(fullscreen_region, ImVec2(static_cast<float>(size.x), static_cast<float>(size.y)), scale);

            if (fullscreen_now) {
                scr->scale_x = static_cast<float>(scale.x);
                scr->scale_y = static_cast<float>(scale.y);
            } else {
                scr->scale_x = ImGui::GetWindowSize().x / rotated_size.x;
                scr->scale_y = scr->scale_x;

                last_scale = scr->scale_x;
            }

            ImVec2 scaled_no_dsa;
            ImVec2 scaled_dsa;

            if (fullscreen_now) {
                scaled_no_dsa.x = static_cast<float>(size.x * scale.x);
                scaled_no_dsa.y = static_cast<float>(size.y * scale.y);

                const eka2l1::vec2 org_screen_size = scr->size();

                scaled_dsa.x = static_cast<float>(org_screen_size.x * scale.x);
                scaled_dsa.y = static_cast<float>(org_screen_size.y * scale.y);

                if (scr->ui_rotation % 180 == 0) {
                    winpos.x += (fullscreen_region.x - scaled_no_dsa.x) / 2;
                    winpos.y += (fullscreen_region.y - scaled_no_dsa.y) / 2;
                } else {
                    winpos.x += (fullscreen_region.x - scaled_no_dsa.y) / 2;
                    winpos.y += (fullscreen_region.y - scaled_no_dsa.x) / 2;
                }
            } else {
                scaled_no_dsa = ImGui::GetWindowSize();
                scaled_no_dsa.y -= ImGui::GetCurrentWindow()->TitleBarHeight();

                const eka2l1::vec2 size_dsa_org = scr->size();

                scaled_dsa.x = static_cast<float>(size_dsa_org.x) * scr->scale_x;
                scaled_dsa.y = static_cast<float>(size_dsa_org.y) * scr->scale_y;
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

    static void ws_window_top_user_selected_callback(void *userdata) {
        epoc::window_top_user *top = reinterpret_cast<epoc::window_top_user *>(userdata);
        ImGui::Text("Type:           Top client");
    }

    static void ws_window_user_selected_callback(void *userdata) {
        epoc::window_user *user = reinterpret_cast<epoc::window_user *>(userdata);

        ImGui::Text("Type:           Client");
        ImGui::Text("Client handle:  0x%08X", user->client_handle);
        ImGui::Text("Position:       { %d, %d }", user->pos.x, user->pos.y);
        ImGui::Text("Extent:         %dx%d", user->size.x, user->size.y);

        switch (user->win_type) {
        case epoc::window_type::backed_up: {
            ImGui::Text("Client type:    Backed up");
            break;
        }

        case epoc::window_type::blank: {
            ImGui::Text("Client type:    Blank");
            break;
        }

        case epoc::window_type::redraw: {
            ImGui::Text("Client type:    Redraw");
            break;
        }

        default:
            break;
        }

        std::string flags = "Flags:          ";

        if (user->flags == 0) {
            flags += "None";
        } else {
            if (user->flags & epoc::window_user::flags_visible) {
                flags += "Visible, ";
            }

            if (user->flags & epoc::window_user::flags_active) {
                flags += "Active, ";
            }

            if (user->flags & epoc::window_user::flags_non_fading) {
                flags += "Non-fading, ";
            }

            flags.pop_back();
            flags.pop_back();
        }

        ImGui::Text("%s", flags.c_str());
        ImGui::Separator();

        if (user->driver_win_id) {
            const eka2l1::vec2 size = user->size;
            ImGui::Image(reinterpret_cast<ImTextureID>(user->driver_win_id), ImVec2(static_cast<float>(size.x), static_cast<float>(size.y)));
        }
    }

    static void ws_window_group_selected_callback(void *userdata) {
        epoc::window_group *group = reinterpret_cast<epoc::window_group *>(userdata);
        const std::string name = common::ucs2_to_utf8(group->name);

        ImGui::Text("Name:           %s", name.c_str());
        ImGui::Text("Type:           Group");
        ImGui::Text("Client handle:  0x%08X", group->client_handle);
        ImGui::Text("Priority:       %d", group->priority);
    }

    static void iterate_through_layer(epoc::window *win, selected_window_callback_function *func, void **userdata) {
        epoc::window *child = win->child;
        selected_window_callback_function to_assign = nullptr;
        std::string node_name = "";

        while (child) {
            switch (child->type) {
            case epoc::window_kind::group: {
                node_name = common::ucs2_to_utf8(reinterpret_cast<epoc::window_group *>(child)->name);
                to_assign = ws_window_group_selected_callback;
                break;
            }

            case epoc::window_kind::client: {
                node_name = fmt::format("{}", reinterpret_cast<epoc::window_user *>(child)->id);
                to_assign = ws_window_user_selected_callback;
                break;
            }

            case epoc::window_kind::top_client: {
                node_name = "Top client";
                to_assign = ws_window_top_user_selected_callback;
                break;
            }

            default:
                break;
            }

            if (to_assign) {
                const bool result = ImGui::TreeNode(node_name.c_str());

                if (ImGui::IsItemClicked()) {
                    *func = to_assign;
                    *userdata = child;
                }

                if (result) {
                    iterate_through_layer(child, func, userdata);
                    ImGui::TreePop();
                }
            }

            child = child->sibling;
        }
    }

    static void ws_screen_selected_callback(void *userdata) {
        epoc::screen *scr = reinterpret_cast<epoc::screen *>(userdata);
        ImGui::Text("Screen number      %d", scr->number);

        if (scr->screen_texture) {
            eka2l1::vec2 size = scr->size();
            ImGui::Image(reinterpret_cast<ImTextureID>(scr->screen_texture), ImVec2(static_cast<float>(size.x), static_cast<float>(size.y)));
        }
    }

    void imgui_debugger::show_windows_tree() {
        if (!winserv) {
            return;
        }

        if (ImGui::Begin("Window tree", &should_show_window_tree)) {
            ImGui::Columns(2);

            epoc::screen *scr = winserv->get_screens();

            for (int i = 0; scr && scr->screen_texture; i++, scr = scr->next) {
                const std::string screen_name = fmt::format("Screen {}", i);

                const bool result = ImGui::TreeNode(screen_name.c_str());

                if (ImGui::IsItemClicked()) {
                    selected_callback = ws_screen_selected_callback;
                    selected_callback_data = scr;
                }

                if (result) {
                    iterate_through_layer(scr->root.get(), &selected_callback, &selected_callback_data);
                    ImGui::TreePop();
                }
            }

            if (selected_callback) {
                ImGui::NextColumn();

                if (ImGui::BeginChild("##EditMenu")) {
                    selected_callback(selected_callback_data);
                    ImGui::EndChild();
                }
            }

            ImGui::Columns(1);
            ImGui::End();
        }
    }

    void imgui_debugger::show_about() {
        if (!phony_icon) {
            static constexpr const char *PHONY_PATH = "resources\\phony.png";
            phony_icon = renderer::load_texture_from_file_standalone(sys->get_graphics_driver(),
                PHONY_PATH, false, &phony_size.x, &phony_size.y);
        }

        if (ImGui::Begin("About!", &should_show_about)) {
            ImGui::Columns(2);

            if (phony_icon) {
                ImGui::Image(reinterpret_cast<ImTextureID>(phony_icon), ImVec2(static_cast<float>(phony_size.x), static_cast<float>(phony_size.y)));
            }

            ImGui::NextColumn();

            if (ImGui::BeginChild("##EKA2L1CreditsText")) {
                ImGui::Text("EKA2L1 - SYMBIAN OS EMULATOR");
                ImGui::Separator();
                ImGui::Text("(C) 2018- EKA2L1 Team.");
                ImGui::Text("Thank you for using the emulator!");
                ImGui::Separator();
                ImGui::Text("Honors:");
                ImGui::Text("- florastamine");
                ImGui::Text("- HadesD (Ichiro)");
                ImGui::Text("- quanshousio");
                ImGui::Text("- claimmore");
                ImGui::Text("- stranno");
                ImGui::EndChild();
            }

            ImGui::Columns(1);

            ImGui::End();
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

    void imgui_debugger::show_debugger(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width, std::uint32_t fb_height) {
        if (font_to_use) {
            ImGui::PushFont(font_to_use);
        }

        show_menu();
        handle_shortcuts();
        show_screens();
        show_errors();

        if (should_show_empty_device_warn) {
            show_empty_device_warn();
            return;
        }

        if (should_warn_touch_disabled) {
            show_touchscreen_disabled_warn();
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

        if (font_to_use) {
            ImGui::PopFont();
        }
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
