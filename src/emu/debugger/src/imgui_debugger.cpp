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

#include <drivers/graphics/emu_window.h> // For scancode

#include <config/config.h>
#include <manager/device_manager.h>
#include <manager/manager.h>
#include <manager/rpkg.h>

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

namespace eka2l1 {
    static void language_property_change_handler(void *userdata, service::property *prop) {
        imgui_debugger *debugger = reinterpret_cast<imgui_debugger *>(userdata);
        config::state *conf = debugger->get_config();

        conf->language = static_cast<int>(debugger->get_language_from_property(prop));
        conf->serialize();

        prop->add_data_change_callback(userdata, language_property_change_handler);
    }

    imgui_debugger::imgui_debugger(eka2l1::system *sys, imgui_logger *logger, app_launch_function app_launch)
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
        , request_key(false)
        , key_set(false)
        , selected_package_index(0xFFFFFFFF)
        , debug_thread_id(0)
        , app_launch(app_launch)
        , selected_callback(nullptr)
        , selected_callback_data(nullptr)
        , phony_icon(0)
        , active_screen(0) {
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
            alserv = reinterpret_cast<eka2l1::applist_server *>(kern->get_by_name<service::server>("!AppListServer"));
            winserv = reinterpret_cast<eka2l1::window_server *>(kern->get_by_name<service::server>("!Windowserver"));
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
            KEY_NUM5,
            KEY_F1,
            KEY_F2
        };
        key_binder_state.target_key_name = {
            "KEY_UP",
            "KEY_DOWN",
            "KEY_LEFT",
            "KEY_RIGHT",
            "KEY_NUM5",
            "KEY_F1",
            "KEY_F2"
        };
        key_binder_state.key_bind_name = {
            "KEY_UP",
            "KEY_DOWN",
            "KEY_LEFT",
            "KEY_RIGHT",
            "KEY_NUM5",
            "KEY_F1",
            "KEY_F2"
        };
        key_binder_state.need_key = std::vector<bool>(key_binder_state.BIND_NUM, false);
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

        ImGui::Text("Background");
        ImGui::Separator();

        ImGui::Text("Path         ");
        ImGui::SameLine();
        ImGui::InputText("##Background", conf->bkg_path.data(), conf->bkg_path.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();

        if (ImGui::Button("Change")) {
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

        ImGui::Text("Transparency ");
        ImGui::SameLine();
        ImGui::SliderInt("##BackgroundTransparency", &conf->bkg_transparency, 0, 255);

        ImGui::Text("UI Scale     ");
        ImGui::SameLine();
        const bool ret = ImGui::InputFloat("##UIScale", &conf->ui_scale, 0.1f);
        if (ret && conf->ui_scale <= 1e-6) {
            conf->ui_scale = 0.5;
        }

        ImGui::NewLine();
        ImGui::Text("Font");
        ImGui::Separator();

        ImGui::Text("Path         ");
        ImGui::SameLine();
        ImGui::InputText("##FontPath", conf->font_path.data(), conf->font_path.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();

        if (ImGui::Button("Replace")) {
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
        ImGui::Text("Debugging");
        ImGui::Separator();

        const float col2 = ImGui::GetWindowSize().x / 2;

        ImGui::Checkbox("CPU Read", &conf->log_read);
        ImGui::SameLine(col2);
        ImGui::Checkbox("CPU write", &conf->log_write);

        ImGui::Checkbox("IPC", &conf->log_ipc);
        ImGui::SameLine(col2);
        ImGui::Checkbox("Symbian API", &conf->log_passed);

        ImGui::Checkbox("System calls", &conf->log_svc);
        ImGui::SameLine(col2);
        ImGui::Checkbox("Accurate IPC timing", &conf->accurate_ipc_timing);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Improve the accuracy of system, but may results in slowdown.");
        }

        ImGui::Checkbox("Enable btrace", &conf->enable_btrace);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable kernel tracing that is used in driver. Slowdown expected on enable");
        }

        ImGui::NewLine();
        ImGui::Text("System");
        ImGui::Separator();

        ImGui::Text("CPU");
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

        ImGui::Text("Device");
        ImGui::SameLine(col2);
        ImGui::PushItemWidth(col2 - 10);

        const auto &dvcs = sys->get_manager_system()->get_device_manager()->get_devices();
        const std::string preview_info = dvcs.empty() ? "No device present" : dvcs[conf->device].model + " (" + dvcs[conf->device].firmware_code + ")";

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
                }
            }

            ImGui::EndCombo();
        }

        if (!dvcs.empty()) {
            ImGui::PopItemWidth();

            ImGui::Text("Language");
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
        }
    }

    void
    imgui_debugger::show_pref_system() {
        const float col2 = ImGui::GetWindowSize().x / 3;

        auto draw_path_change = [&](const char *title, const char *button, std::string &dat) {
            ImGui::Text("%s", title);
            ImGui::SameLine(col2);

            ImGui::PushItemWidth(col2 * 2 - 75);
            ImGui::InputText("##Path", dat.data(), dat.size(), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();

            ImGui::SameLine(col2 * 3 - 65);
            ImGui::PushItemWidth(30);

            if (ImGui::Button(button)) {
                on_pause_toogle(true);

                drivers::open_native_dialog(
                    sys->get_graphics_driver(),
                    "", [&](const char *res) {
                        dat = res;
                    },
                    true);

                should_pause = false;
                on_pause_toogle(false);
            }

            ImGui::PopItemWidth();
        };

        draw_path_change("Data storage", "Change##1", conf->storage);

        ImGui::Separator();

        ImGui::NewLine();
        ImGui::Text("%s", "Key binding");
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
                    switch (key_evt.type_) {
                    case drivers::input_event_type::key:
                        window_server::key_input_map[key_evt.key_.code_] = key_binder_state.target_key[i];
                        key_binder_state.key_bind_name[i] = std::to_string(key_evt.key_.code_);
                        map_set = true;
                        break;
                    case drivers::input_event_type::button:
                        window_server::button_input_map[std::make_pair(key_evt.button_.controller_, key_evt.button_.button_)] = key_binder_state.target_key[i];
                        key_binder_state.key_bind_name[i] = std::to_string(key_evt.button_.controller_) + ":" + std::to_string(key_evt.button_.button_);
                        map_set = true;
                        break;
                    default:
                        break;
                    }
                    if (map_set) {
                        key_binder_state.need_key[i] = false;
                        request_key = false;
                        key_set = false;
                    }
                }
                ImGui::Button("waiting for key");
            } else {
                if (ImGui::Button(key_binder_state.key_bind_name[i].c_str())) {
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

        ImGui::Text("Battery level");
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
        ImGui::Text("Signal level");

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
        ImGui::Begin("Preferences", &should_show_preferences);

        using show_func = std::function<void(imgui_debugger *)>;
        static std::vector<std::pair<std::string, show_func>> all_prefs = {
            { "General", &imgui_debugger::show_pref_general },
            { "System", &imgui_debugger::show_pref_system },
            { "Personalisation", &imgui_debugger::show_pref_personalisation },
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

        ImGui::NewLine();
        ImGui::NewLine();

        ImGui::SameLine(std::max(0.0f, ImGui::GetWindowSize().x / 2 - 30.0f));
        ImGui::PushItemWidth(30);

        if (ImGui::Button("Save")) {
            conf->serialize();
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
        ImGui::Begin("Packages", &should_package_manager, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Package")) {
                if (ImGui::MenuItem("Install")) {
                    do_install_package();
                }

                if (ImGui::MenuItem("Remove", nullptr, &should_package_manager_remove)) {
                    should_package_manager_remove = true;
                }

                if (ImGui::MenuItem("File lists", nullptr, &should_package_manager_display_file_list)) {
                    should_package_manager_display_file_list = true;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
        ImGui::Columns(4);

        ImGui::SetColumnWidth(0, ImGui::CalcTextSize("0x11111111").x + 20.0f);

        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "UID");
        ImGui::NextColumn();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Name");
        ImGui::NextColumn();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Vendor");
        ImGui::NextColumn();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Drive");
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
                if (ImGui::MenuItem("File lists")) {
                    should_package_manager_display_file_list = true;
                }

                if (ImGui::MenuItem("Remove")) {
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

            ImGui::OpenPopup("Info");

            if (ImGui::BeginPopupModal("Info", &should_package_manager_remove)) {
                if (pkg == nullptr) {
                    ImGui::Text("No package selected.");
                } else {
                    std::string ask_question = "Are you sure you want to remove the package ("
                        + common::ucs2_to_utf8(pkg->name) + ")?";

                    ImGui::Text("%s", ask_question.c_str());
                }

                ImGui::NewLine();

                ImGui::SameLine(ImGui::CalcItemWidth() / 2 + 20.0f);

                if (ImGui::Button("Yes")) {
                    should_package_manager_remove = false;
                    should_package_manager_display_file_list = false;

                    if (pkg != nullptr) {
                        manager->uninstall_package(pkg->id);
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("No")) {
                    should_package_manager_remove = false;
                }

                ImGui::EndPopup();
            }
        }
    }

    void imgui_debugger::show_installer_text_popup() {
        ImGui::OpenPopup("Installer Text Popup");
        ImGuiIO &io = ImGui::GetIO();

        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x / 4, io.DisplaySize.y / 3));

        if (ImGui::BeginPopupModal("Installer Text Popup")) {
            ImGui::Text("%s", installer_text);
            ImGui::NewLine();

            const float button_size = 30.0f;
            float total_width_button_occupied = button_size;

            if (!should_package_manager_display_one_button_only) {
                total_width_button_occupied += button_size + 4.0f;
            }

            ImGui::SameLine((ImGui::GetWindowWidth() - total_width_button_occupied) / 2);

            if (ImGui::Button("Yes")) {
                installer_text_result = true;
                installer_cond.notify_one();

                should_package_manager_display_installer_text = false;
            }

            ImGui::SameLine();

            if (!should_package_manager_display_one_button_only && ImGui::Button("No")) {
                installer_text_result = false;
                installer_cond.notify_one();

                should_package_manager_display_installer_text = false;
            }

            ImGui::EndPopup();
        }
    }

    void imgui_debugger::show_installer_choose_lang_popup() {
        ImGui::OpenPopup("Choose the language for this package");

        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x / 4, io.DisplaySize.y / 3));

        if (ImGui::BeginPopupModal("Choose the language for this package")) {
            for (int i = 0; i < installer_lang_size; i++) {
                if (ImGui::Selectable(num_to_lang(installer_langs[i]), installer_current_lang_idx == i)) {
                    installer_current_lang_idx = i;
                }
            }

            ImGui::NewLine();

            const float button_size = 30.0f;
            float total_width_button_occupied = button_size * 2 + 4.0f;

            ImGui::SameLine((ImGui::GetWindowWidth() - total_width_button_occupied) / 2);

            if (ImGui::Button("OK")) {
                installer_lang_choose_result = installer_langs[installer_current_lang_idx];
                installer_cond.notify_one();

                should_package_manager_display_language_choose = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
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
        static ImVec2 BUTTON_SIZE = ImVec2(50, 20);

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

        ImGui::OpenPopup("Install device wizard");
        ImGui::SetNextWindowSize(ImVec2(640, 180), ImGuiCond_Once);

        if (ImGui::BeginPopupModal("Install device wizard")) {
            switch (device_wizard_state.stage) {
            case device_wizard::WELCOME_MESSAGE:
                ImGui::TextWrapped("Welcome to install device wizard. Me, En will guide you through this!");
                device_wizard_state.should_continue = true;
                break;

            case device_wizard::SPECIFY_FILES: {
                ImGui::TextWrapped("Please specify the repackage file (RPKG):");
                ImGui::InputText("##RPKGPath", device_wizard_state.current_rpkg_path.data(),
                    device_wizard_state.current_rpkg_path.size(), ImGuiInputTextFlags_ReadOnly);

                ImGui::SameLine();

                if (ImGui::Button("Change##1")) {
                    on_pause_toogle(true);

                    drivers::open_native_dialog(sys->get_graphics_driver(), "rpkg", [&](const char *result) {
                        device_wizard_state.current_rpkg_path = result;
                        device_wizard_state.should_continue_temps[0] = eka2l1::exists(result);
                        device_wizard_state.should_continue = (device_wizard_state.should_continue_temps[1]
                            && eka2l1::exists(result));
                    });

                    should_pause = false;
                    on_pause_toogle(false);
                }

                ImGui::Separator();

                ImGui::TextWrapped("Please specify the ROM file:");
                ImGui::InputText("##ROMPath", device_wizard_state.current_rom_path.data(),
                    device_wizard_state.current_rom_path.size(), ImGuiInputTextFlags_ReadOnly);

                ImGui::SameLine();

                if (ImGui::Button("Change##2")) {
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
                ImGui::TextWrapped("Please wait while we install the device!");

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

                bool extract_rpkg_state = device_wizard_state.extract_rpkg_done.load();
                bool copy_rom_state = device_wizard_state.copy_rom_done.load();

                ImGui::Checkbox("Extract the RPKG", &extract_rpkg_state);
                ImGui::Checkbox("Copy the ROM", &copy_rom_state);

                if (device_wizard_state.failure) {
                    ImGui::TextWrapped("Something wrong happens during the install! Please check the log!");
                    device_wizard_state.should_continue = true;
                }

                ImGui::PopItemFlag();

                break;
            }

            case device_wizard::ENDING: {
                ImGui::TextWrapped("Thank you for checking by. En hopes you have a good time!");
                ImGui::TextWrapped("For any problem, please report to the developers by opening issues!");

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
            ImGui::SameLine((ImGui::GetWindowSize().x - (BUTTON_SIZE.x * 2)) / 2);

            if (device_wizard_state.stage == device_wizard::ENDING) {
                if (ImGui::Button("OK", BUTTON_SIZE)) {
                    should_show_install_device_wizard = false;
                    device_wizard_state.stage = device_wizard::FINAL_FOR_REAL;
                }
            } else {
                if (ImGuiButtonToggle("Yes", BUTTON_SIZE, device_wizard_state.should_continue)) {
                    device_wizard_state.stage = static_cast<device_wizard::device_wizard_stage>(static_cast<int>(device_wizard_state.stage) + 1);
                    device_wizard_state.should_continue = false;

                    if (device_wizard_state.stage == device_wizard::INSTALL) {
                        manager::device_manager *manager = sys->get_manager_system()->get_device_manager();

                        device_wizard_state.install_thread = std::make_unique<std::thread>([](
                                                                                               manager::device_manager *mngr, device_wizard *wizard, config::state *conf) {
                            std::atomic<int> progress;
                            std::string firmware_code;

                            bool result = eka2l1::loader::install_rpkg(mngr, wizard->current_rpkg_path,
                                add_path(conf->storage, "drives/z/"), firmware_code, progress);

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

                if ((device_wizard_state.stage != device_wizard::INSTALL) && ImGui::Button("No", BUTTON_SIZE)) {
                    should_show_install_device_wizard = false;
                    device_wizard_state.stage = device_wizard::WELCOME_MESSAGE;
                }
            }

            ImGui::EndPopup();
        }
    }

    void imgui_debugger::show_menu() {
        if (ImGui::BeginMainMenuBar()) {
            conf->menu_height = ImGui::GetWindowSize().y;

            if (ImGui::BeginMenu("File")) {
                ImGui::MenuItem("Logger", "CTRL+SHIFT+L", &should_show_logger);

                bool last_show_launch = should_show_app_launch;
                ImGui::MenuItem("Launch apps", "CTRL+R", &should_show_app_launch);

                if ((last_show_launch != should_show_app_launch) && last_show_launch == false) {
                    should_still_focus_on_keyboard = true;
                }

                manager::device_manager *dvc_mngr = sys->get_manager_system()->get_device_manager();
                if (!dvc_mngr->get_current()) {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

                    ImGui::MenuItem("Packages");

                    if (ImGui::IsItemHovered()) {
                        ImGui::PopStyleVar();
                        ImGui::SetTooltip("Please install a device to access the package manager!");
                    } else {
                        ImGui::PopStyleVar();
                    }
                } else {
                    if (ImGui::BeginMenu("Packages")) {
                        ImGui::MenuItem("Install", nullptr, &should_install_package);
                        ImGui::MenuItem("List", nullptr, &should_package_manager);
                        ImGui::EndMenu();
                    }
                }

                ImGui::Separator();

                ImGui::MenuItem("Install device", nullptr, &should_show_install_device_wizard);

                ImGui::Separator();

                ImGui::MenuItem("Preferences", nullptr, &should_show_preferences);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Debugger")) {
                ImGui::MenuItem("Pause", "CTRL+P", &should_pause);
                ImGui::MenuItem("Stop", nullptr, &should_stop);

                ImGui::Separator();

                ImGui::MenuItem("Disassembler", nullptr, &should_show_disassembler);

                if (ImGui::BeginMenu("Objects")) {
                    ImGui::MenuItem("Threads", nullptr, &should_show_threads);
                    ImGui::MenuItem("Mutexs", nullptr, &should_show_mutexs);
                    ImGui::MenuItem("Chunks", nullptr, &should_show_chunks);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Services")) {
                    ImGui::MenuItem("Window tree", nullptr, &should_show_window_tree);
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                bool fullscreen = renderer->is_fullscreen();
                ImGui::MenuItem("Fullscreen", nullptr, &fullscreen);
                renderer->set_fullscreen(fullscreen);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                ImGui::MenuItem("About", nullptr, &should_show_about);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
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
        ImGui::Begin("App launcher", &should_show_app_launch);

        if (alserv) {
            std::vector<apa_app_registry> &registerations = alserv->get_registerations();
            const float uid_col_size = ImGui::CalcTextSize("00000000").x + 30.0f;

            {
                ImGui::Text("Search ");
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

                ImGui::TextColored(GUI_COLOR_TEXT_TITLE, " Name");
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
                            app_launch(registerations[i].mandatory_info.app_path.to_std_string(nullptr));
                        }
                    }
                }

                if (app_search_box.Filters.empty())
                    clipper.End();

                ImGui::EndChild();
            }
            ImGui::Columns(1);
        } else {
            ImGui::Text("App List Server is not available. Install a device to enable it.");
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

        ImGui::OpenPopup("A wild error appears!");

        if (ImGui::BeginPopupModal("A wild error appears!")) {
            ImGui::Text("%s", first_error.c_str());
            ImGui::Text("Please attach the log and report this to a developer!");

            if (ImGui::Button("OK")) {
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

            const std::string name = fmt::format("Screen {}", scr->number);
            eka2l1::vec2 size = scr->current_mode().size;

            scr->screen_mutex.lock();
            ImVec2 rotated_size(size.x + 15.0f, size.y + 35.0f);

            if (scr->ui_rotation % 180 == 90) {
                rotated_size.x = size.y + 15.0f;
                rotated_size.y = size.x + 35.0f;
            }

            ImVec2 fullscreen_region = ImVec2(io.DisplaySize.x, io.DisplaySize.y - 60.0f);

            const float fullscreen_start_x = 0.0f;
            const float fullscreen_start_y = 30.0f;

            if (fullscreen_now) {
                ImGui::SetNextWindowSize(fullscreen_region);
                ImGui::SetNextWindowPos(ImVec2(fullscreen_start_x, fullscreen_start_y));
            } else {
                ImGui::SetNextWindowSize(rotated_size);
            }

            if (fullscreen_now)
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGui::Begin(name.c_str(), nullptr, fullscreen_now ? (ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize) : 0);

            if (ImGui::IsWindowFocused()) {
                active_screen = scr->number;
            }

            ImVec2 winpos = ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin();

            if (fullscreen_now ? ImGui::BeginPopupContextWindow(nullptr, ImGuiMouseButton_Right) : ImGui::BeginPopupContextItem(nullptr, ImGuiMouseButton_Right)) {
                bool orientation_lock = scr->orientation_lock;
                if (ImGui::BeginMenu("Orientation lock", false)) {
                    orientation_lock = !orientation_lock;
                    ImGui::EndMenu();
                }

                scr->set_orientation_lock(sys->get_graphics_driver(), orientation_lock);

                if (ImGui::BeginMenu("Rotation")) {
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

                if (ImGui::BeginMenu("Refresh rate")) {
                    int current_refresh_rate = scr->refresh_rate;
                    ImGui::SliderInt("##RefreshRate", &current_refresh_rate, 0, 120);

                    if (current_refresh_rate != scr->refresh_rate) {
                        scr->refresh_rate = static_cast<std::uint8_t>(current_refresh_rate);
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndPopup();
            }

            eka2l1::vec2 scale(0, 0);
            get_nice_scale_by_integer(fullscreen_region, ImVec2(static_cast<float>(size.x), static_cast<float>(size.y)), scale);

            if (fullscreen_now) {
                scr->scale = scale;
            } else {
                scr->scale = { 1, 1 };
            }

            if (fullscreen_now) {
                size.x *= scale.x;
                size.y *= scale.y;

                if (scr->ui_rotation % 180 == 0) {
                    winpos.x += (fullscreen_region.x - size.x) / 2;
                    winpos.y += (fullscreen_region.y - size.y) / 2;
                } else {
                    winpos.x += (fullscreen_region.x - size.y) / 2;
                    winpos.y += (fullscreen_region.y - size.x) / 2;
                }
            }

            scr->absolute_pos.x = static_cast<int>(winpos.x);
            scr->absolute_pos.y = static_cast<int>(winpos.y);

            draw_a_screen(reinterpret_cast<ImTextureID>(scr->screen_texture), winpos,
                ImVec2(static_cast<float>(size.x), static_cast<float>(size.y)), scr->ui_rotation);

            if (scr->dsa_texture) {
                const eka2l1::vec2 size_dsa = scr->size();
                const int rotation = (scr->current_mode().rotation + scr->ui_rotation) % 360;

                draw_a_screen(reinterpret_cast<ImTextureID>(scr->dsa_texture), winpos,
                    ImVec2(static_cast<float>(size_dsa.x * scr->scale.x), static_cast<float>(size_dsa.y * scr->scale.y)),
                    rotation);
            }

            scr->screen_mutex.unlock();

            ImGui::End();

            if (fullscreen_now) {
                ImGui::PopStyleVar();
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
                ImGui::EndChild();
            }

            ImGui::Columns(1);

            ImGui::End();
        }
    }

    void imgui_debugger::show_empty_device_warn() {
        ImGui::OpenPopup("##NoDevicePresent");
        if (ImGui::BeginPopupModal("##NoDevicePresent", &should_show_empty_device_warn)) {
            ImGui::Text("You have not installed any device. Please follow the installation instructions on EKA2L1's wiki page.");

            ImGui::NewLine();
            ImGui::SameLine((ImGui::GetWindowSize().x - 30.0f) / 2);

            ImGui::SetNextItemWidth(30.0f);

            if (ImGui::Button("OK")) {
                should_show_empty_device_warn = false;
            }

            ImGui::EndPopup();
        }
    }

    void imgui_debugger::show_debugger(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width, std::uint32_t fb_height) {
        show_menu();
        handle_shortcuts();
        show_screens();
        show_errors();

        if (should_show_empty_device_warn) {
            show_empty_device_warn();
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

        if (should_show_about) {
            show_about();
        }

        on_pause_toogle(should_pause);

        if (should_show_logger) {
            logger->draw("Logger", &should_show_logger);
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
}
