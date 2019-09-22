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

#include <drivers/graphics/backend/emu_window_glfw.h>
#include <drivers/graphics/emu_window.h>

namespace eka2l1 {
    namespace drivers {
        std::unique_ptr<emu_window> new_emu_window(window_type win_type) {
            switch (win_type) {
            case window_type::glfw: {
                return std::make_unique<emu_window_glfw3>();
            }
            }

            return nullptr;
        }

        bool init_window_library(window_type win_type) {
            switch (win_type) {
            case window_type::glfw:
                return glfwInit() == GLFW_TRUE ? true : false;

            default:
                break;
            }

            return false;
        }

        bool destroy_window_library(window_type win_type) {
            switch (win_type) {
            case window_type::glfw:
                glfwTerminate();
                return true;

            default:
                break;
            }

            return false;
        }
    }
}
