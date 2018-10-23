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

#include <debugger/debugger.h>
#include <debugger/logger.h>
#include <debugger/mem_editor.h>

#include <imgui.h>

#include <core/core.h>

#define RGBA_TO_FLOAT(r, g, b, a) ImVec4(r / 255.0, g / 255.0, b / 255.0, a / 255.0)

const ImVec4 GUI_COLOR_TEXT_MENUBAR = RGBA_TO_FLOAT(242, 150, 58, 255);
const ImVec4 GUI_COLOR_TEXT_MENUBAR_OPTIONS = RGBA_TO_FLOAT(242, 150, 58, 255);
const ImVec4 GUI_COLOR_TEXT_TITLE = RGBA_TO_FLOAT(247, 198, 51, 255);
const ImVec4 GUI_COLOR_TEXT = RGBA_TO_FLOAT(255, 255, 255, 255);

namespace eka2l1 {
    debugger::debugger(eka2l1::system *sys, std::shared_ptr<imgui_logger> logger)
        : sys(sys), logger(logger) {
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

    void debugger::show_threads() {
        if (ImGui::Begin("Threads", &should_show_threads)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s    %-32s", "ID", 
                "Thread name", "State");

            for (const auto &obj: sys->get_kernel_system()->objects) {
                if (obj && obj->get_object_type() == kernel::object_type::thread) {
                    std::string obj_name = obj->name();
                    thread_ptr thr = std::dynamic_pointer_cast<kernel::thread>(obj);

                    ImGui::TextColored(GUI_COLOR_TEXT, "0x%08lX    %-32s    %-32s", obj->unique_id(),
                        obj_name.c_str(), thread_state_to_string(thr->current_state()));
                }
            }
        }

        ImGui::End();
    }

    void debugger::show_mutexs() {
        if (ImGui::Begin("Mutexs", &should_show_mutexs)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s", "ID", 
                "Mutex name");

            for (const auto &obj: sys->get_kernel_system()->objects) {
                if (obj && obj->get_object_type() == kernel::object_type::mutex) {
                    std::string obj_name = obj->name();

                    ImGui::TextColored(GUI_COLOR_TEXT, "0x%08lX    %-32s", obj->unique_id(),
                        obj_name.c_str());
                }
            }
        }

        ImGui::End();
    }

    void debugger::show_chunks() {
        if (ImGui::Begin("Chunks", &should_show_chunks)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s    %-8s    %-8s    %-8s    %-8s      %-32s", "ID", 
                "Chunk name", "Base", "Bottom", "Top", "Max", "Creator process");

            for (const auto &obj: sys->get_kernel_system()->objects) {
                if (obj && obj->get_object_type() == kernel::object_type::chunk) {
                    std::string obj_name = obj->name();
                    chunk_ptr chnk = std::dynamic_pointer_cast<kernel::chunk>(obj);
                    std::string process_name = chnk->get_own_process() ?
                        chnk->get_own_process()->name() : "Unknown";

                    ImGui::TextColored(GUI_COLOR_TEXT, "0x%08lX    %-32s    0x%08X    0x%08X    0x%08X    0x%08lX      %-32s", 
                        obj->unique_id(), obj_name.c_str(), chnk->base().ptr_address(), chnk->get_bottom(), chnk->get_top(),
                        chnk->get_max_size(), process_name.c_str());
                }
            }
        }

        ImGui::End();
    }

    void debugger::show_timers() {

    }

    void debugger::show_disassembler() {

    }

    void debugger::show_memory() {
        mem_editor->DrawWindow("Memory Editor");
    }

    void debugger::show_menu() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Advance")) {
                ImGui::MenuItem("Pause", "CTRL+P", &should_pause);
                ImGui::MenuItem("Stop", nullptr, &should_stop);
                ImGui::MenuItem("Load state", nullptr, &should_load_state);
                ImGui::MenuItem("Save state", nullptr, &should_save_state);
                ImGui::MenuItem("Install package", nullptr, &should_install_package);
                ImGui::MenuItem("Logger", "CTRL+SHIFT+L", &should_show_logger);
                ImGui::MenuItem("Memory Viewer", "CTRL+M", &(mem_editor->Open));

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Objects")) {
                ImGui::MenuItem("Threads", nullptr, &should_show_threads);
                ImGui::MenuItem("Mutexs", nullptr, &should_show_mutexs);
                ImGui::MenuItem("Chunks", nullptr, &should_show_chunks);
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Disassembler")) {
                show_disassembler();
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void debugger::show_debugger(std::uint32_t width, std::uint32_t height
            , std::uint32_t fb_width, std::uint32_t fb_height) {
        ImGuiIO &io = ImGui::GetIO();
        
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        io.DisplayFramebufferScale = ImVec2(
            width > 0 ? ((float)fb_width / width) : 0, height > 0 ? ((float)fb_height / height) : 0);

        ImGui::NewFrame();

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

        if (should_show_logger) {
            logger->draw("Logger", &should_show_logger);
        }

        ImGui::EndFrame();
    }
}