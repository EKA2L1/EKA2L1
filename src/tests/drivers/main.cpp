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

#include <common/log.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "graphics/scene.h"
#include "graphics/texture.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

scenes scenes_;

static void key_callback(GLFWwindow *win, int key, int scancodes, int action, int mods) {

}

int main(int argc, char **argv) {
    // Setup the log
    eka2l1::log::setup_log(nullptr);

    // Setup a new window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *win = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "EKA2L1 drivers test", nullptr, nullptr);

    if (!win) {
        LOG_ERROR("Can't create GLFW window!");
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(win, key_callback);
    glfwMakeContextCurrent(win);

    // Initialize GLAD
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        LOG_ERROR("Can't initialize GLAD!");
        return -1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Setup scenes
    scenes_.driver = drivers::create_graphics_driver(eka2l1::drivers::graphic_api::opengl, vec2(WINDOW_WIDTH, WINDOW_HEIGHT));
    scenes_.gr_api = eka2l1::drivers::graphic_api::opengl;
    scenes_.spritebatch = create_sprite_batch(scenes_.gr_api);

    int crr_scene = 0;

    while (!glfwWindowShouldClose(win)) {   
        switch (crr_scene) {
        case 0: {
            bitmap_upload_and_draw_scene(&scenes_);
            break;
        }

        default:
            break;
        }

        if (scenes_.gr_api == eka2l1::drivers::graphic_api::opengl) {
            // Support for OpenGL < 4.3
            GLenum err = 0;

            err = glGetError();
            while (err != GL_NO_ERROR) {
                LOG_ERROR("Error in OpenGL operation: {}", err);
                err = glGetError();
            }
        }
        
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
