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

#include <services/window/window.h>
#include <services/window/classes/winbase.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/common.h>

#include <kernel/kernel.h>
#include <kernel/chunk.h>
#include <kernel/mutex.h>
#include <kernel/thread.h>
#include <kernel/timer.h>

#include <cpu/arm_utils.h>
#include <disasm/disasm.h>
#include <system/epoc.h>
#include <common/cvt.h>
#include <imgui.h>

#include <mutex>
#include <thread>

namespace eka2l1 {    
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

                for (std::uint32_t pc = ctx.get_pc() - 12, i = 0; i < 12; i++) {
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
                ImGui::Text("PC: 0x%08X         SP: 0x%08X         LR: 0x%08X", ctx.get_pc(), ctx.get_sp(), ctx.get_lr());
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
}