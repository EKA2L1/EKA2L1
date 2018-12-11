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
#include <debugger/mem_editor.h>

#include <imgui.h>

#include <epoc/epoc.h>
#include <disasm/disasm.h>

#include <epoc/kernel/libmanager.h>

#include <common/cvt.h>

#define RGBA_TO_FLOAT(r, g, b, a) ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f)

const ImVec4 GUI_COLOR_TEXT_MENUBAR = RGBA_TO_FLOAT(242.0f, 150.0f, 58.0f, 255.0f);
const ImVec4 GUI_COLOR_TEXT_MENUBAR_OPTIONS = RGBA_TO_FLOAT(242.0f, 150.0f, 58.0f, 255.0f);
const ImVec4 GUI_COLOR_TEXT_TITLE = RGBA_TO_FLOAT(247.0f, 198.0f, 51.0f, 255.0f);
const ImVec4 GUI_COLOR_TEXT = RGBA_TO_FLOAT(255, 255, 255, 255);
const ImVec4 GUI_COLOR_TEXT_SELECTED = RGBA_TO_FLOAT(125.0f, 251.0f, 143.0f, 255.0f);

namespace eka2l1 {
    imgui_debugger::imgui_debugger(eka2l1::system *sys, std::shared_ptr<imgui_logger> logger)
        : sys(sys)
        , logger(logger)
        , should_stop(false) 
        , should_pause(false) {
        mem_editor = std::make_shared<MemoryEditor>(sys->get_kernel_system());
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

            for (const auto &obj : sys->get_kernel_system()->objects) {
                if (obj && obj->get_object_type() == kernel::object_type::thread) {
                    std::string obj_name = obj->name();
                    thread_ptr thr = std::dynamic_pointer_cast<kernel::thread>(obj);

                    chunk_ptr chnk = thr->get_stack_chunk();

                    ImGui::TextColored(GUI_COLOR_TEXT, "0x%08lX    %-32s    %-32s    0x%08X", obj->unique_id(),
                        obj_name.c_str(), thread_state_to_string(thr->current_state()), chnk->base().ptr_address());
                }
            }
        }

        ImGui::End();
    }

    void imgui_debugger::show_mutexs() {
        if (ImGui::Begin("Mutexs", &should_show_mutexs)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s", "ID",
                "Mutex name");

            const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock);

            for (const auto &obj : sys->get_kernel_system()->objects) {
                if (obj && obj->get_object_type() == kernel::object_type::mutex) {
                    std::string obj_name = obj->name();

                    ImGui::TextColored(GUI_COLOR_TEXT, "0x%08lX    %-32s", obj->unique_id(),
                        obj_name.c_str());
                }
            }
        }

        ImGui::End();
    }

    void imgui_debugger::show_chunks() {
        if (ImGui::Begin("Chunks", &should_show_chunks)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s    %-8s    %-8s    %-8s    %-8s      %-32s", "ID",
                "Chunk name", "Base", "Bottom", "Top", "Max", "Creator process");

            const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock);

            for (const auto &obj : sys->get_kernel_system()->objects) {
                if (obj && obj->get_object_type() == kernel::object_type::chunk) {
                    std::string obj_name = obj->name();
                    chunk_ptr chnk = std::dynamic_pointer_cast<kernel::chunk>(obj);
                    std::string process_name = chnk->get_own_process() ? chnk->get_own_process()->name() : "Unknown";

                    ImGui::TextColored(GUI_COLOR_TEXT, "0x%08lX    %-32s    0x%08X    0x%08X    0x%08X    0x%08lX      %-32s",
                        obj->unique_id(), obj_name.c_str(), chnk->base().ptr_address(), chnk->get_bottom(), chnk->get_top(),
                        chnk->get_max_size(), process_name.c_str());
                }
            }
        }

        ImGui::End();
    }

    void imgui_debugger::show_timers() {
    }

    void imgui_debugger::show_disassembler() {
        if (ImGui::Begin("Disassembler", &should_show_disassembler)) {
            std::vector<thread_ptr> threads;
            thread_ptr debug_thread = nullptr;

            // Thread can't be deleted, but there can be data race when a new thread is added
            {
                const std::lock_guard<std::mutex> guard(sys->get_kernel_system()->kern_lock);

                for (const auto &obj : sys->get_kernel_system()->objects) {
                    if (obj && obj->get_object_type() == kernel::object_type::thread) {
                        threads.push_back(std::dynamic_pointer_cast<kernel::thread>(obj));

                        if (debug_thread_id == obj->unique_id()) {
                            debug_thread = threads.back();
                        }
                    }
                }
            }

            if (!debug_thread) {
                if (threads.size() == 0) {
                    ImGui::End();
                    return;
                }

                debug_thread = threads[0];
                debug_thread_id = threads[0]->unique_id();
            }

            std::string thr_name = debug_thread->name();

            if (ImGui::BeginCombo("Thread", debug_thread ? thr_name.c_str() : "Thread")) {
                for (std::size_t i = 0; i < threads.size(); i++) {
                    std::string cr_thrname = threads[i]->name() + " (ID " + common::to_string(threads[i]->unique_id()) + ")";
          
                    if (ImGui::Selectable(cr_thrname.c_str(), threads[i]->unique_id() == debug_thread_id)) {
                        debug_thread_id = threads[i]->unique_id();
                        debug_thread = threads[i];
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::NewLine();

            if (debug_thread) {
                arm::arm_interface::thread_context &ctx = debug_thread->get_thread_context();

                // Using the table for also situation like local JIT code from guest
                page_table &table = debug_thread->owning_process()->get_page_table();

                for (std::uint32_t pc = ctx.pc - 12, i = 0; i < 12; i++) {
                    void *codeptr = table.get_ptr(pc);

                    if (!codeptr) {
                        break;
                    }

                    const std::string dis = sys->get_disasm()->disassemble(reinterpret_cast<const std::uint8_t *>(codeptr), ctx.cpsr & 0x20 ? 2 : 4, 
                        pc, ctx.cpsr & 0x20);

                    if (dis.find("svc") == std::string::npos) {
                        ImGui::Text("0x%08x: %-10u    %s", pc, *reinterpret_cast<std::uint32_t *>(codeptr), dis.c_str());
                    } else {
                        const std::uint32_t svc_num = std::stoul(dis.substr(5), nullptr, 16);
                        const std::string svc_call_name = sys->get_lib_manager()->svc_funcs.find(svc_num) != sys->get_lib_manager()->svc_funcs.end() ? 
                            sys->get_lib_manager()->svc_funcs[svc_num].name : "Unknown";

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

    void imgui_debugger::show_memory() {
        mem_editor->DrawWindow("Memory Editor");
    }

    void imgui_debugger::show_breakpoint_list() {
        ImGui::Begin("Breakpoints", &should_show_breakpoint_list);

        ImGuiStyle& style = ImGui::GetStyle();
        
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

    void imgui_debugger::show_menu() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Miscs")) {
                ImGui::MenuItem("Logger", "CTRL+SHIFT+L", &should_show_logger);
                ImGui::MenuItem("Load state", nullptr, &should_load_state);
                ImGui::MenuItem("Save state", nullptr, &should_save_state);
                ImGui::MenuItem("Install package", nullptr, &should_install_package);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Advance")) {
                ImGui::MenuItem("Pause", "CTRL+P", &should_pause);
                ImGui::MenuItem("Stop", nullptr, &should_stop);

                ImGui::Separator();

                ImGui::MenuItem("Memory Viewer", "CTRL+M", &(mem_editor->Open));
                ImGui::MenuItem("Disassembler", nullptr, &should_show_disassembler);
                ImGui::MenuItem("Breakpoints", nullptr, &should_show_breakpoint_list);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Objects")) {
                ImGui::MenuItem("Threads", nullptr, &should_show_threads);
                ImGui::MenuItem("Mutexs", nullptr, &should_show_mutexs);
                ImGui::MenuItem("Chunks", nullptr, &should_show_chunks);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void imgui_debugger::show_debugger(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width, std::uint32_t fb_height) {
        show_menu();

        if (should_show_threads) {
            show_threads();
        }

        if (should_show_mutexs) {
            show_mutexs();
        }

        if (should_show_chunks) {
            show_chunks();
        }

        if (mem_editor->Open) {
            show_memory();
        }

        if (should_show_disassembler) {
            show_disassembler();
        }

        if (should_show_breakpoint_list) {
            show_breakpoint_list();
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