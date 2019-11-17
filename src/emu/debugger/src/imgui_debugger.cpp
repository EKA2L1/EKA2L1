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

#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <debugger/renderer/renderer.h>

#include <imgui.h>

#include <disasm/disasm.h>
#include <epoc/epoc.h>

#include <epoc/kernel.h>
#include <epoc/kernel/libmanager.h>
#include <epoc/kernel/thread.h>

#include <epoc/services/applist/applist.h>
#include <epoc/services/window/window.h>
#include <epoc/services/window/classes/winbase.h>
#include <epoc/services/window/classes/wingroup.h>
#include <epoc/services/window/classes/winuser.h>

#include <drivers/graphics/emu_window.h> // For scancode

#include <manager/config.h>
#include <manager/device_manager.h>
#include <manager/manager.h>

#include <common/cvt.h>
#include <common/platform.h>

#include <nfd.h>

#include <mutex>

#define RGBA_TO_FLOAT(r, g, b, a) ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f)

const ImVec4 GUI_COLOR_TEXT_MENUBAR = RGBA_TO_FLOAT(242.0f, 150.0f, 58.0f, 255.0f);
const ImVec4 GUI_COLOR_TEXT_MENUBAR_OPTIONS = RGBA_TO_FLOAT(242.0f, 150.0f, 58.0f, 255.0f);
const ImVec4 GUI_COLOR_TEXT_TITLE = RGBA_TO_FLOAT(247.0f, 198.0f, 51.0f, 255.0f);
const ImVec4 GUI_COLOR_TEXT = RGBA_TO_FLOAT(255, 255, 255, 255);
const ImVec4 GUI_COLOR_TEXT_SELECTED = RGBA_TO_FLOAT(125.0f, 251.0f, 143.0f, 255.0f);

namespace eka2l1 {
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
        , should_show_breakpoint_list(false)
        , should_show_preferences(false)
        , should_package_manager(false)
        , should_package_manager_display_file_list(false)
        , should_package_manager_remove(false)
        , should_package_manager_display_installer_text(false)
        , should_package_manager_display_language_choose(false)
        , should_install_package(false)
        , should_show_app_launch(false)
        , selected_package_index(0xFFFFFFFF)
        , debug_thread_id(0)
        , app_launch(app_launch)
        , selected_callback(nullptr)
        , selected_callback_data(nullptr) {
        // Setup hook
        manager::package_manager *pkg_mngr = sys->get_manager_system()->get_package_manager();

        pkg_mngr->show_text = [&](const char *text_buf) -> bool {
            installer_text = text_buf;
            should_package_manager_display_installer_text = true;

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
            while (true && !install_thread_should_stop) {
                while (auto result = install_list.pop()) {
                    if (!install_thread_should_stop) {
                        get_sys()->install_package(common::utf8_to_ucs2(result.value()), drive_c);
                    }
                }

                std::unique_lock<std::mutex> ul(install_thread_mut);
                install_thread_cond.wait(ul);
            }
        });
        
        auto applist_svr_ptr = std::reinterpret_pointer_cast<eka2l1::applist_server>(sys->get_kernel_system()
            ->get_by_name<service::server>("!AppListServer"));

        auto winserv_ptr = std::reinterpret_pointer_cast<eka2l1::window_server>(sys->get_kernel_system()
            ->get_by_name<service::server>("!Windowserver"));

        alserv = applist_svr_ptr.get();
        winserv = winserv_ptr.get();
    }

    imgui_debugger::~imgui_debugger() {
        install_thread_should_stop = true;
        install_thread_cond.notify_one();
        install_thread->join();
    }

    manager::config_state *imgui_debugger::get_config() {
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
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s    %-32s    %-32s", "ID",
                "Thread name", "State", "Stack");

            const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock);

            for (const auto &thr : sys->get_kernel_system()->threads) {
                chunk_ptr chnk = thr->get_stack_chunk();

                ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X    %-32s    %-32s    0x%08X", thr->unique_id(),
                    thr->name().c_str(), thread_state_to_string(thr->current_state()), chnk->base().ptr_address());
            }
        }

        ImGui::End();
    }

    void imgui_debugger::show_mutexs() {
        if (ImGui::Begin("Mutexs", &should_show_mutexs)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s", "ID",
                "Mutex name");

            const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock);

            for (const auto &mutex : sys->get_kernel_system()->mutexes) {
                ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X    %-32s", mutex->unique_id(),
                    mutex->name().c_str());
            }
        }

        ImGui::End();
    }

    void imgui_debugger::show_chunks() {
        if (ImGui::Begin("Chunks", &should_show_chunks)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s    %-8s       %-8s    %-8s      %-32s", "ID",
                "Chunk name", "Base", "Committed", "Max", "Creator process");

            const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock);

            for (const auto &chnk : sys->get_kernel_system()->chunks) {
                std::string process_name = chnk->get_own_process() ? chnk->get_own_process()->name() : "Unknown";

                ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X    %-32s       0x%08X       0x%08lX    0x%08lX      %-32s",
                    chnk->unique_id(), chnk->name().c_str(), chnk->base().ptr_address(), chnk->committed(),
                    chnk->max_size(), process_name.c_str());
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
                const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock);

                if (kern->threads.size() == 0) {
                    ImGui::End();
                    return;
                }

                debug_thread = *kern->threads.begin();
                debug_thread_id = debug_thread->unique_id();
            } else {
                debug_thread = kern->get_by_id<kernel::thread>(debug_thread_id);
            }

            std::string thr_name = debug_thread->name();

            if (ImGui::BeginCombo("Thread", debug_thread ? thr_name.c_str() : "Thread")) {
                for (std::size_t i = 0; i < kern->threads.size(); i++) {
                    std::string cr_thrname = kern->threads[i]->name() + " (ID " + common::to_string(kern->threads[i]->unique_id()) + ")";

                    if (ImGui::Selectable(cr_thrname.c_str(), kern->threads[i]->unique_id() == debug_thread_id)) {
                        debug_thread_id = kern->threads[i]->unique_id();
                        debug_thread = kern->threads[i];
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::NewLine();

            if (debug_thread) {
                arm::arm_interface::thread_context &ctx = debug_thread->get_thread_context();

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
                        const std::string svc_call_name = sys->get_lib_manager()->svc_funcs.find(svc_num) != sys->get_lib_manager()->svc_funcs.end() ? sys->get_lib_manager()->svc_funcs[svc_num].name : "Unknown";

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

    void imgui_debugger::show_breakpoint_list() {
        ImGui::Begin("Breakpoints", &should_show_breakpoint_list);

        ImGuiStyle &style = ImGui::GetStyle();

        const float height_separator = style.ItemSpacing.y;
        float footer_height = height_separator + ImGui::GetFrameHeightWithSpacing() * 1;

        ImGui::BeginChild("##breakpoints_scroll", ImVec2(0, -footer_height), false, ImGuiWindowFlags_NoMove);
        ImGuiListClipper clipper(static_cast<int>(breakpoints.size()), ImGui::GetTextLineHeight());

        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-8s        %-32s", "Number",
            "Address");

        for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            std::string bkpt_info;
            bkpt_info.resize(22);

            sprintf(&bkpt_info[0], "0x%08X    0x%08X", i, breakpoints[i].addr);

            bool pushed = false;

            if (modify_element == i) {
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_SELECTED);
                pushed = true;
            }

            if (ImGui::Selectable(bkpt_info.c_str(), i == modify_element)) {
                modify_element = i;
            }

            if (pushed) {
                ImGui::PopStyleColor();
            }
        }

        clipper.End();
        ImGui::EndChild();

        ImGui::Separator();

        std::string buf;
        buf.resize(18);

        if (ImGui::InputText("", &buf[0], 18)) {
            try {
                if (buf.substr(0, 2) == "0x") {
                    buf = buf.c_str();
                    addr = std::stoul(buf, 0, 16);
                } else {
                    addr = std::atol(buf.data());
                }
            } catch (...) {
                addr = 0;
            }

            addr -= addr % 2;
        }

        ImGui::SameLine();

        if (ImGui::Button("Add")) {
            if (addr != 0) {
                set_breakpoint(addr, false, true);
                addr = 0;
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Remove")) {
            if (addr != 0) {
                unset_breakpoint(addr);
                addr = 0;
            } else if (modify_element != -1) {
                unset_breakpoint(breakpoints[modify_element].addr);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Deselect")) {
            modify_element = -1;
        }

        ImGui::End();
    }

    template <typename F>
    bool file_dialog(const char *filter, F callback, const bool is_picking_folder = false) {
        nfdchar_t *out_path = nullptr;
        nfdresult_t result = is_picking_folder ? NFD_PickFolder(nullptr, &out_path) : NFD_OpenDialog(filter, nullptr, &out_path);

        if (result == NFD_OKAY) {
            callback(out_path);
        }

        free(out_path);

        if (result == NFD_OKAY) {
            return true;
        }

        return false;
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

            file_dialog("png,jpg,bmp", [&](const char *result) {
                conf->bkg_path = result;
                renderer->change_background(result);

                conf->serialize();
            });

            should_pause = false;
            on_pause_toogle(false);
        }

        ImGui::Text("Transparency ");
        ImGui::SameLine();
        ImGui::SliderInt("##BackgroundTransparency", &conf->bkg_transparency, 0, 255);

        ImGui::NewLine();
        ImGui::Text("Font");
        ImGui::Separator();

        ImGui::Text("Path         ");
        ImGui::SameLine();
        ImGui::InputText("##FontPath", conf->font_path.data(), conf->font_path.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();

        if (ImGui::Button("Replace")) {
            on_pause_toogle(true);

            file_dialog("ttf", [&](const char *result) {
                conf->font_path = result;
                conf->serialize();
            });

            should_pause = false;
            on_pause_toogle(false);
        }

        ImGui::Separator();
    }

    void imgui_debugger::show_pref_general() {
        ImGui::Text("Logging");
        ImGui::Separator();

        const float col2 = ImGui::GetWindowSize().x / 2;

        ImGui::Checkbox("CPU Read", &conf->log_read);
        ImGui::SameLine(col2);
        ImGui::Checkbox("CPU write", &conf->log_write);

        ImGui::Checkbox("CPU Code", &conf->log_code);
        ImGui::SameLine(col2);
        ImGui::Checkbox("IPC", &conf->log_ipc);

        ImGui::Checkbox("Symbian API", &conf->log_passed);
        ImGui::SameLine(col2);
        ImGui::Checkbox("System calls", &conf->log_svc);

        ImGui::NewLine();
        ImGui::Text("System");
        ImGui::Separator();

        ImGui::Text("CPU");
        ImGui::SameLine(col2);
        ImGui::PushItemWidth(col2 - 10);

        if (ImGui::BeginCombo("##CPUCombo", (conf->cpu_backend == eka2l1::unicorn_jit_backend_name) ? "Unicorn" : "Dynarmic")) {
            if (ImGui::Selectable("Unicorn")) {
                conf->cpu_backend = eka2l1::unicorn_jit_backend_name;
                conf->serialize();
            }

            if (ImGui::Selectable("Dynarmic")) {
                conf->cpu_backend = eka2l1::dynarmic_jit_backend_name;
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

        if (ImGui::BeginCombo("##Devicescombo", preview_info.c_str())) {
            for (std::size_t i = 0; i < dvcs.size(); i++) {
                const std::string device_info = dvcs[i].model + " (" + dvcs[i].firmware_code + ")";
                if (ImGui::Selectable(device_info.c_str())) {
                    conf->device = static_cast<int>(i);
                    conf->serialize();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
    }

    void imgui_debugger::show_pref_mounting() {
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

                file_dialog("", [&](const char *res) {
                    dat = res;
                },
                    true);

                should_pause = false;
                on_pause_toogle(false);
            }

            ImGui::PopItemWidth();
        };

        draw_path_change("Data storage", "Change##1", conf->storage);
    }

    void imgui_debugger::show_pref_hal() {
        ImGui::Text("Screen Size");
        const float col2 = ImGui::GetWindowSize().x / 3;

        ImGui::SameLine(col2);
        ImGui::Text("X");
        ImGui::SameLine(col2 + 10);
        ImGui::PushItemWidth(col2 - 20);
        ImGui::InputInt("##ScreenSizeXInput", &conf->display_size_x_pixs);
        ImGui::PopItemWidth();

        ImGui::SameLine(col2 * 2);
        ImGui::Text("Y");
        ImGui::SameLine(col2 * 2 + 10);
        ImGui::PushItemWidth(col2 - 20);
        ImGui::InputInt("##ScreenSizeYInput", &conf->display_size_y_pixs);
        ImGui::PopItemWidth();

        ImGui::Text("RAM size");
        ImGui::SameLine(col2);

        int mb_initial = static_cast<int>(conf->maximum_ram / common::MB(1));
        ImGui::PushItemWidth(col2 * 2 - 30);
        ImGui::SliderInt("MB", &mb_initial, 64, 512);
        ImGui::PopItemWidth();
        conf->maximum_ram = static_cast<std::uint32_t>(mb_initial * common::MB(1));
    }

    void imgui_debugger::show_preferences() {
        ImGui::Begin("Preferences", &should_show_preferences);

        using show_func = std::function<void(imgui_debugger *)>;
        static std::vector<std::pair<std::string, show_func>> all_prefs = {
            { "General", &imgui_debugger::show_pref_general },
            { "Mounting", &imgui_debugger::show_pref_mounting },
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

        file_dialog("sis,sisx", [&](const char *res) {
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

        if (ImGui::BeginPopupModal("Installer Text Popup")) {
            ImGui::Text("%s", installer_text);

            if (ImGui::Button("Yes")) {
                installer_text_result = true;
                installer_cond.notify_one();

                should_package_manager_display_installer_text = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("No")) {
                installer_text_result = false;
                installer_cond.notify_one();

                should_package_manager_display_installer_text = false;
            }

            ImGui::EndPopup();
        }
    }

    void imgui_debugger::show_installer_choose_lang_popup() {
        ImGui::OpenPopup("Choose the language for this package");

        if (ImGui::BeginPopupModal("Choose the language for this package")) {
            for (int i = 0; i < installer_lang_size; i++) {
                if (ImGui::Selectable(num_to_lang(installer_langs[i]), installer_current_lang_idx == i)) {
                    installer_current_lang_idx = i;
                }
            }

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

    void imgui_debugger::show_menu() {
        if (ImGui::BeginMainMenuBar()) {
            conf->menu_height = ImGui::GetWindowSize().y;

            if (ImGui::BeginMenu("File")) {
                ImGui::MenuItem("Logger", "CTRL+SHIFT+L", &should_show_logger);
                ImGui::MenuItem("Launch apps", "CTRL+R", &should_show_app_launch);
                ImGui::MenuItem("Packages", nullptr, &should_package_manager);
                ImGui::MenuItem("Install package", nullptr, &should_install_package);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Debugger")) {
                ImGui::MenuItem("Pause", "CTRL+P", &should_pause);
                ImGui::MenuItem("Stop", nullptr, &should_stop);

                ImGui::Separator();

                ImGui::MenuItem("Disassembler", nullptr, &should_show_disassembler);
                ImGui::MenuItem("Breakpoints", nullptr, &should_show_breakpoint_list);

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

            if (ImGui::BeginMenu("Settings")) {
                ImGui::MenuItem("Preferences", nullptr, &should_show_preferences);
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
                    const std::string name = " " + common::ucs2_to_utf8(registerations[i].mandatory_info.long_caption.to_std_string(nullptr));
                    const std::string uid_name = common::to_string(registerations[i].mandatory_info.uid, std::hex);

                    if (app_search_box.PassFilter(name.c_str())) {
                        ImGui::Text(uid_name.c_str());
                        ImGui::NextColumn();

                        ImGui::Text(name.c_str());
                        ImGui::NextColumn();

                        if (ImGui::IsItemClicked()) {
                            // Launch app!
                            should_show_app_launch = false;
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
            ImGui::Text("App List Server is not available");
        }

        ImGui::End();
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

        ImGui::OpenPopup("A wild error appears!");

        if (ImGui::BeginPopupModal("A wild error appears!")) {
            ImGui::Text(first_error.c_str());
            ImGui::Text("Please attach the log and report this to a developer!");

            if (ImGui::Button("OK")) {
                const std::lock_guard<std::mutex> guard(errors_mut);
                error_queue.pop();
            }

            ImGui::EndPopup();
        }
    }

    void imgui_debugger::show_screens() {
        // Iterate through all screen from window server
        if (!winserv) {
            return;
        }

        epoc::screen *scr = winserv->get_screens();

        for (int i = 0; scr && scr->screen_texture; i++, scr = scr->next) {
            const std::string name = fmt::format("Screen {}", i);
            const eka2l1::vec2 size = scr->size();

            ImGui::SetNextWindowSize(ImVec2(size.x + 15.0f, size.y + 35.0f));
            ImGui::Begin(name.c_str());
            ImGui::Image(reinterpret_cast<ImTextureID>(scr->screen_texture), ImVec2(size.x, size.y));
            ImGui::End();
        }
    }
        
    static void ws_window_top_user_selected_callback(void *userdata) {
        epoc::window_top_user *top = reinterpret_cast<epoc::window_top_user*>(userdata);
        ImGui::Text("Type:           Top client");
    }

    static void ws_window_user_selected_callback(void *userdata) {
        epoc::window_user *user = reinterpret_cast<epoc::window_user*>(userdata);
        
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

        ImGui::Text(flags.c_str());
        ImGui::Separator();

        if (user->driver_win_id) {
            const eka2l1::vec2 size = user->size;
            ImGui::Image(reinterpret_cast<ImTextureID>(user->driver_win_id), ImVec2(size.x, size.y)); 
        }
    }

    static void ws_window_group_selected_callback(void *userdata) {
        epoc::window_group *group = reinterpret_cast<epoc::window_group*>(userdata);
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
                node_name = common::ucs2_to_utf8(reinterpret_cast<epoc::window_group*>(child)->name);
                to_assign = ws_window_group_selected_callback;
                break;
            }

            case epoc::window_kind::client: {
                node_name = fmt::format("{}", reinterpret_cast<epoc::window_user*>(win)->id);
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
        epoc::screen *scr = reinterpret_cast<epoc::screen*>(userdata);
        ImGui::Text("Screen number      %d", scr->number);

        if (scr->screen_texture) {
            eka2l1::vec2 size = scr->size();
            ImGui::Image(reinterpret_cast<ImTextureID>(scr->screen_texture), ImVec2(size.x, size.y)); 
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
    
    void imgui_debugger::show_debugger(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width, std::uint32_t fb_height) {
        show_menu();
        handle_shortcuts();
        show_screens();
        show_errors();

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

        if (should_show_breakpoint_list) {
            show_breakpoint_list();
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
