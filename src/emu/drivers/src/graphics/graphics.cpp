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

#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <drivers/graphics/graphics.h>

#include <common/log.h>
#include <glad/glad.h>

namespace eka2l1::drivers {
    static void gl_post_callback_for_error(const char *name, void *funcptr, int len_args, ...) {
        GLenum error_code;
        error_code = glad_glGetError();

        if (error_code != GL_NO_ERROR) {
            LOG_ERROR("{} encounters error {}", name, error_code);
        }
    }

    bool init_graphics_library(graphic_api api) {
        switch (api) {
        case graphic_api::opengl: {
            gladLoadGL();
            glad_set_post_callback(gl_post_callback_for_error);

            return true;
        }

        default:
            break;
        }

        return false;
    }

    graphics_driver_ptr create_graphics_driver(const graphic_api api) {
        switch (api) {
        case graphic_api::opengl: {
            return std::make_unique<ogl_graphics_driver>();
        }

        default:
            break;
        }

        return nullptr;
    }
}
