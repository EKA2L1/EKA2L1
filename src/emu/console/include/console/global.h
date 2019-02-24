/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

#include <epoc/epoc.h>

#include <imgui.h>
#include <yaml-cpp/yaml.h>

namespace eka2l1 {
    struct imgui_logger;
    class imgui_debugger;
}

using namespace eka2l1;

extern std::unique_ptr<eka2l1::system> symsys;
extern arm_emulator_type jit_type;

extern std::uint16_t gdb_port;
extern std::uint8_t device_to_use; ///< Device that will be used

extern std::mutex ui_debugger_mutex;
extern ImGuiContext *ui_debugger_context;

extern std::mutex lock;
extern std::condition_variable cond;

extern std::string mount_z;

extern std::atomic<bool> should_quit;
extern std::atomic<bool> should_pause;

extern bool debugger_quit;

extern std::mutex ui_debugger_mutex;
extern ImGuiContext *ui_debugger_context;

extern bool ui_window_mouse_down[5];

extern std::shared_ptr<eka2l1::imgui_logger> logger;
extern std::shared_ptr<eka2l1::imgui_debugger> debugger;
