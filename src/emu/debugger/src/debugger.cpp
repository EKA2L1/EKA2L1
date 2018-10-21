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
#include <imgui.h>

#include <core/core.h>

#define RGBA_TO_FLOAT(r, g, b, a) ImVec4(r / 255.0, g / 255.0, b / 255.0, a / 255.0)

const ImVec4 GUI_COLOR_TEXT_MENUBAR = RGBA_TO_FLOAT(242, 150, 58, 255);
const ImVec4 GUI_COLOR_TEXT_MENUBAR_OPTIONS = RGBA_TO_FLOAT(242, 150, 58, 255);
const ImVec4 GUI_COLOR_TEXT_TITLE = RGBA_TO_FLOAT(247, 198, 51, 255);
const ImVec4 GUI_COLOR_TEXT = RGBA_TO_FLOAT(255, 255, 255, 255);

namespace eka2l1 {
    void debugger::show_threads() {
        if (ImGui::Begin("Threads", &should_show_threads)) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-32s", "ID", 
                "Thread name");

            for (const auto &obj: sys->get_kernel_system()->objects) {
                if (obj->get_object_type() == kernel::object_type::thread) {
                    std::string obj_name = obj->name();

                    ImGui::TextColored(GUI_COLOR_TEXT, "0x%16.8lX    %-32s", obj->unique_id(),
                        obj_name.c_str());
                }
            }
        }

        ImGui::End();
    }

    void debugger::show_mutexs() {

    }

    void debugger::show_chunks() {

    }

    void debugger::show_timers() {

    }

    void debugger::show_disassembler() {

    }

    void debugger::show_menu() {
        if (ImGui::BeginMainMenuBar()) {
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

        ImGui::EndFrame();
    }
}