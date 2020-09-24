/*
 * Copyright (c) 2021 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <drivers/graphics/backend/cursor_glfw.h>

namespace eka2l1::drivers {
    cursor_glfw::cursor_glfw(GLFWcursor *cursor)
        : cursor_(cursor) {
    }

    cursor_glfw::~cursor_glfw() {
        if (cursor_) {
            glfwDestroyCursor(cursor_);
        }
    }

    std::unique_ptr<cursor> cursor_controller_glfw::create(const cursor_type type) {
        int standard_type = GLFW_ARROW_CURSOR;

        switch (type) {
        case cursor_type_arrow:
            standard_type = GLFW_ARROW_CURSOR;
            break;

        case cursor_type_hand:
            standard_type = GLFW_HAND_CURSOR;
            break;

        case cursor_type_ibeam:
            standard_type = GLFW_IBEAM_CURSOR;
            break;

        case cursor_type_hresize:
            standard_type = GLFW_HRESIZE_CURSOR;
            break;

        case cursor_type_vresize:
            standard_type = GLFW_VRESIZE_CURSOR;
            break;

        case cursor_type_allresize:
            standard_type = GLFW_RESIZE_ALL_CURSOR;
            break;

        case cursor_type_nesw_resize:
            standard_type = GLFW_RESIZE_NESW_CURSOR;
            break;

        case cursor_type_nwse_resize:
            standard_type = GLFW_RESIZE_NWSE_CURSOR;
            break;

        default:
            return nullptr;
        }

        GLFWcursor *cur = glfwCreateStandardCursor(standard_type);
        if (cur) {
            return std::make_unique<cursor_glfw>(cur);
        }

        return nullptr;
    }
}