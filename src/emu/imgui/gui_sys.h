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
#pragma once

#include <ImguiWindowsFileIO.hpp>
#include <imgui.h>

#include <functional>
#include <future>
#include <map>
#include <vector>

// A Async GUI System based on ImGui
namespace eka2l1 {
    using draw_func = std::function<bool()>;
    using draw_callback = std::function<void(bool)>;
    using draw_id = int;

    struct draw_request {
        draw_id did;
        draw_func dfunc;
        draw_callback dcb;
    };

    class gui_system {
        std::map<draw_id, draw_request> dreqs;

    public:
        std::string open_dialogue(const std::vector<std::string> filters) {
            fileIOWindow()

                std::string res_path;
        }
    };
}

