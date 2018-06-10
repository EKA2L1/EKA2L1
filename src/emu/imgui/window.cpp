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
#include "window.h"

namespace eka2l1 {
    namespace imgui {
        GLFWwindow *open_window(const std::string &title, const uint32_t width,
            const uint32_t height) {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

            GLFWwindow *win = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
            glfwMakeContextCurrent(win);

            gladLoadGL();

            return win;
        }

        void close_window(GLFWwindow *window) {
            glfwSetWindowShouldClose(window, true);
        }

        void destroy_window(GLFWwindow *window) {
            glfwDestroyWindow(window);
        }
    }
}

