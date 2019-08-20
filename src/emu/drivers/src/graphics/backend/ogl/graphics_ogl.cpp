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
#include <fstream>
#include <sstream>

#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <glad/glad.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD

namespace eka2l1::drivers {
    ogl_graphics_driver::ogl_graphics_driver()
        : shared_graphics_driver(graphic_api::opengl) {
        init_graphics_library(eka2l1::drivers::graphic_api::opengl);
    }

    static constexpr char *sprite_norm_v_path = "resources//sprite_norm.vert";
    static constexpr char *sprite_norm_f_path = "resources//sprite_norm.frag";

    void ogl_graphics_driver::do_init() {
        sprite_program = std::make_unique<ogl_shader>(sprite_norm_v_path, sprite_norm_f_path);

        static GLfloat vertices[] = {
            // Pos      // Tex
            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,

            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f
        };

        glGenVertexArrays(1, &sprite_vao);
        glGenBuffers(1, &sprite_vbo);

        glBindVertexArray(sprite_vao);
        glBindBuffer(GL_ARRAY_BUFFER, sprite_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        color_loc = sprite_program->get_uniform_location("u_color").value();
        projection_loc = sprite_program->get_uniform_location("u_projection").value();
        model_loc = sprite_program->get_uniform_location("u_model").value();
    }

    void ogl_graphics_driver::draw_bitmap(command_helper &helper) {
        // Get bitmap to draw
        drivers::handle to_draw = 0;
        helper.pop(to_draw);

        bitmap *bmp = get_bitmap(to_draw);

        if (!bmp) {
            LOG_ERROR("Invalid bitmap handle to draw");
            return;
        }

        sprite_program->use(this);
        glBindVertexArray(sprite_vao);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bmp->tex->texture_handle()));

        // Build model matrix
        eka2l1::rect draw_rect;
        helper.pop(draw_rect);

        glm::mat4 model_matrix = glm::mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
        model_matrix = glm::translate(model_matrix, glm::vec3(static_cast<float>(draw_rect.top.x), static_cast<float>(draw_rect.top.y), 0.0f));

        // Make size and rotation
        model_matrix = glm::translate(model_matrix, glm::vec3(0.5f * static_cast<float>(draw_rect.size.x), 0.5f * static_cast<float>(draw_rect.size.y), 0.0f));
        model_matrix = glm::rotate(model_matrix, 0.0f, glm::vec3(0.0f, 0.0f, 1.0f));
        model_matrix = glm::translate(model_matrix, glm::vec3(-0.5f * static_cast<float>(draw_rect.size.x), -0.5f * static_cast<float>(draw_rect.size.y), 0.0f));

        glUniform4fv(projection_loc, 4, glm::value_ptr(projection_matrix));
        glUniform4fv(model_loc, 4, glm::value_ptr(model_matrix));

        const GLfloat color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glUniform4fv(color_loc, 1, color);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    void ogl_graphics_driver::set_invalidate(command_helper &helper) {
        bool enable = false;
        helper.pop(enable);

        if (enable) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    void ogl_graphics_driver::invalidate_rect(command_helper &helper) {
        eka2l1::rect inv_rect;
        helper.pop(inv_rect);
    
        glScissor(inv_rect.top.x, binding->tex->get_size().y - inv_rect.top.y, inv_rect.size.x, inv_rect.size.y);
    }
}
