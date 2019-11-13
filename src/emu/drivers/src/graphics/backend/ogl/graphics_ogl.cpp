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

#include <drivers/graphics/backend/ogl/common_ogl.h>
#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <glad/glad.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD

namespace eka2l1::drivers {
    ogl_graphics_driver::ogl_graphics_driver()
        : shared_graphics_driver(graphic_api::opengl)
        , should_stop(false) {
        init_graphics_library(eka2l1::drivers::graphic_api::opengl);
    }

    static constexpr const char *sprite_norm_v_path = "resources//sprite_norm.vert";
    static constexpr const char *sprite_norm_f_path = "resources//sprite_norm.frag";
    static constexpr const char *fill_v_path = "resources//fill.vert";
    static constexpr const char *fill_f_path = "resources//fill.frag";

    void ogl_graphics_driver::do_init() {
        sprite_program = std::make_unique<ogl_shader>(sprite_norm_v_path, sprite_norm_f_path);
        fill_program = std::make_unique<ogl_shader>(fill_v_path, fill_f_path);

        static GLushort indices[] = {
            0, 1, 2,
            0, 3, 1
        };

        // Make sprite VAO and VBO
        glGenVertexArrays(1, &sprite_vao);
        glGenBuffers(1, &sprite_vbo);
        glBindVertexArray(sprite_vao);
        glBindBuffer(GL_ARRAY_BUFFER, sprite_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)(2 * sizeof(GLfloat)));
        glBindVertexArray(0);

        // Make fill VAO and VBO        
        glGenVertexArrays(1, &fill_vao);
        glGenBuffers(1, &fill_vbo);
        glBindVertexArray(fill_vao);
        glBindBuffer(GL_ARRAY_BUFFER, fill_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid *)0);
        glBindVertexArray(0);

        glGenBuffers(1, &sprite_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sprite_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        color_loc = sprite_program->get_uniform_location("u_color").value_or(-1);
        proj_loc = sprite_program->get_uniform_location("u_proj").value_or(-1);
        model_loc = sprite_program->get_uniform_location("u_model").value_or(-1);
        
        color_loc_fill = fill_program->get_uniform_location("u_color").value_or(-1);
        proj_loc_fill = fill_program->get_uniform_location("u_proj").value_or(-1);
        model_loc_fill = fill_program->get_uniform_location("u_model").value_or(-1);
    }

    void ogl_graphics_driver::bind_swapchain_framebuf() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ogl_graphics_driver::draw_rectangle(command_helper &helper) {
        if (!fill_program) {
            do_init();
        }

        eka2l1::rect fill_rect;
        helper.pop(fill_rect);

        fill_program->use(this);

        // Build model matrix
        glm::mat4 model_matrix = glm::identity<glm::mat4>();
        model_matrix = glm::translate(model_matrix, { fill_rect.top.x, fill_rect.top.y, 0.0f });
        model_matrix = glm::scale(model_matrix, { fill_rect.size.x, fill_rect.size.y, 0.0f });
        
        glUniformMatrix4fv(model_loc_fill, 1, false, glm::value_ptr(model_matrix));
        glUniformMatrix4fv(proj_loc_fill, 1, false, glm::value_ptr(projection_matrix));

        // Supply brush
        glUniform4fv(color_loc_fill, 1, brush_color.elements.data());

        static GLfloat fill_verts_default[] = {
            0.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f,
            1.0f, 1.0f,
        };

        glBindVertexArray(fill_vao);
        glBindBuffer(GL_ARRAY_BUFFER, fill_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fill_verts_default), nullptr, GL_STATIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fill_verts_default), fill_verts_default, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid *)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sprite_ibo);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

        glBindVertexArray(0);
    }
    
    void ogl_graphics_driver::draw_bitmap(command_helper &helper) {
        if (!sprite_program) {
            do_init();
        }

        // Get bitmap to draw
        drivers::handle to_draw = 0;
        helper.pop(to_draw);

        bitmap *bmp = get_bitmap(to_draw);

        if (!bmp) {
            LOG_ERROR("Invalid bitmap handle to draw");
            return;
        }

        eka2l1::vec2 position;
        helper.pop(position);

        sprite_program->use(this);

        // Build texcoords
        eka2l1::rect source_rect;
        helper.pop(source_rect);

        struct sprite_vertex {
            float top[2];
            float coord[2];
        } verts[4];

        static GLfloat verts_default[] = {
            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
        };

        void *vert_pointer = verts_default;

        if (!source_rect.empty()) {
            const float texel_width = 1.0f / bmp->tex->get_size().x;
            const float texel_height = 1.0f / bmp->tex->get_size().y;

            // Bottom left
            verts[0].top[0] = 0.0f;
            verts[0].top[1] = 1.0f;

            verts[0].coord[0] = source_rect.top.x * texel_width;
            verts[0].coord[1] = (source_rect.top.y + source_rect.size.y) * texel_height;
            
            // Top right
            verts[1].top[0] = 1.0f;
            verts[1].top[1] = 0.0f;

            verts[1].coord[0] = (source_rect.top.x + source_rect.size.x) * texel_width;
            verts[1].coord[1] = source_rect.top.y * texel_height;

            // Top left
            verts[2].top[0] = 0.0f;
            verts[2].top[1] = 0.0f;

            verts[2].coord[0] = source_rect.top.x * texel_width;
            verts[2].coord[1] = source_rect.top.y * texel_height;

            // Bottom right
            verts[3].top[0] = 1.0f;
            verts[3].top[1] = 1.0f;

            verts[3].coord[0] = (source_rect.top.x + source_rect.size.x) * texel_width;
            verts[3].coord[1] = (source_rect.top.y + source_rect.size.y) * texel_height;

            vert_pointer = verts;
        }

        glBindVertexArray(sprite_vao);
        glBindBuffer(GL_ARRAY_BUFFER, sprite_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), nullptr, GL_STATIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), vert_pointer, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)(2 * sizeof(GLfloat)));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bmp->tex->texture_handle()));

        // For unknown reason my intel driver go out for an all out attack and garbage the filter...
        // so i have to set it here...
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);    
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Build model matrix
        glm::mat4 model_matrix = glm::identity<glm::mat4>();
        model_matrix = glm::translate(model_matrix, { position.x, position.y, 0.0f });

        if (source_rect.size.x == 0) {
            source_rect.size.x = bmp->tex->get_size().x;
        }

        if (source_rect.size.y == 0) {
            source_rect.size.y = bmp->tex->get_size().y;
        }

        model_matrix = glm::scale(model_matrix, glm::vec3(source_rect.size.x, source_rect.size.y, 0.0f));

        glUniformMatrix4fv(model_loc, 1, false, glm::value_ptr(model_matrix));
        glUniformMatrix4fv(proj_loc, 1, false, glm::value_ptr(projection_matrix));

        // Supply brush
        bool use_brush = false;
        helper.pop(use_brush);

        const GLfloat color[] = { 255.0f, 255.0f, 255.0f, 255.0f };

        if (use_brush) {
            glUniform4fv(color_loc, 1, brush_color.elements.data());
        } else {
            glUniform4fv(color_loc, 1, color);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sprite_ibo);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

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

        glScissor(inv_rect.top.x, current_fb_height - (inv_rect.top.y + inv_rect.size.y),
            inv_rect.size.x, inv_rect.size.y);
    }

    static GLenum prim_mode_to_gl_enum(const graphics_primitive_mode prim_mode) {
        switch (prim_mode) {
        case graphics_primitive_mode::triangles:
            return GL_TRIANGLES;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }

    void ogl_graphics_driver::draw_indexed(command_helper &helper) {
        graphics_primitive_mode prim_mode = graphics_primitive_mode::triangles;
        int count = 0;
        data_format val_type = data_format::word;
        int index_off = 0;
        int vert_off = 0;

        helper.pop(prim_mode);
        helper.pop(count);
        helper.pop(val_type);
        helper.pop(index_off);
        helper.pop(vert_off);

        glDrawElementsBaseVertex(prim_mode_to_gl_enum(prim_mode), count, data_format_to_gl_enum(val_type), reinterpret_cast<GLvoid *>(index_off), vert_off);
    }

    void ogl_graphics_driver::set_viewport(const eka2l1::rect &viewport) {
        glViewport(viewport.top.x, current_fb_height - (viewport.top.y + viewport.size.y), viewport.size.x, viewport.size.y);
    }

    static void set_one(command_helper &helper, GLenum to_set) {
        bool enable = true;
        helper.pop(enable);

        if (enable) {
            glEnable(to_set);
        } else {
            glDisable(to_set);
        }
    }

    void ogl_graphics_driver::set_depth(command_helper &helper) {
        set_one(helper, GL_DEPTH_TEST);
    }

    void ogl_graphics_driver::set_blend(command_helper &helper) {
        set_one(helper, GL_BLEND);
    }

    void ogl_graphics_driver::set_cull(command_helper &helper) {
        set_one(helper, GL_CULL_FACE);
    }

    void ogl_graphics_driver::set_viewport(command_helper &helper) {
        eka2l1::rect viewport;
        helper.pop(viewport);

        set_viewport(viewport);
    }

    static GLenum blend_equation_to_gl_enum(const blend_equation eq) {
        switch (eq) {
        case blend_equation::add:
            return GL_FUNC_ADD;

        case blend_equation::sub:
            return GL_FUNC_SUBTRACT;

        case blend_equation::isub:
            return GL_FUNC_REVERSE_SUBTRACT;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }

    static GLenum blend_factor_to_gl_enum(const blend_factor fac) {
        switch (fac) {
        case blend_factor::one:
            return GL_ONE;

        case blend_factor::zero:
            return GL_ZERO;

        case blend_factor::frag_out_alpha:
            return GL_SRC_ALPHA;

        case blend_factor::one_minus_frag_out_alpha:
            return GL_ONE_MINUS_SRC_ALPHA;

        case blend_factor::current_alpha:
            return GL_DST_ALPHA;

        case blend_factor::one_minus_current_alpha:
            return GL_ONE_MINUS_DST_ALPHA;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }

    void ogl_graphics_driver::blend_formula(command_helper &helper) {
        blend_equation rgb_equation = blend_equation::add;
        blend_equation a_equation = blend_equation::add;
        blend_factor rgb_frag_out_factor = blend_factor::one;
        blend_factor rgb_current_factor = blend_factor::zero;
        blend_factor a_frag_out_factor = blend_factor::one;
        blend_factor a_current_factor = blend_factor::zero;

        helper.pop(rgb_equation);
        helper.pop(a_equation);
        helper.pop(rgb_frag_out_factor);
        helper.pop(rgb_current_factor);
        helper.pop(a_frag_out_factor);
        helper.pop(a_current_factor);

        glBlendEquationSeparate(blend_equation_to_gl_enum(rgb_equation), blend_equation_to_gl_enum(a_equation));
        glBlendFuncSeparate(blend_factor_to_gl_enum(rgb_frag_out_factor), blend_factor_to_gl_enum(rgb_current_factor),
            blend_factor_to_gl_enum(a_frag_out_factor), blend_factor_to_gl_enum(a_current_factor));
    }

    void ogl_graphics_driver::clear(command_helper &helper) {
        std::uint32_t color_to_clear;
        std::uint8_t clear_bits = false;

        helper.pop(color_to_clear);
        helper.pop(clear_bits);

        glClearColor(((color_to_clear & 0xFF000000) >> 24) / 255.0f,
            ((color_to_clear & 0x00FF0000) >> 16) / 255.0f,
            ((color_to_clear & 0x0000FF00) >> 8) / 255.0f,
            (color_to_clear & 0x000000FF) / 255.0f);

        switch (clear_bits) {
        case 3:
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            break;

        case 2:
            glClear(GL_DEPTH_BUFFER_BIT);
            break;

        case 1:
            glClear(GL_COLOR_BUFFER_BIT);
            break;

        default:
            break;
        }
    }

    void ogl_graphics_driver::save_gl_state() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &backup.last_program);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &backup.last_texture);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &backup.last_active_texture);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &backup.last_array_buffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &backup.last_element_array_buffer);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &backup.last_vertex_array);
        glGetIntegerv(GL_BLEND_SRC, &backup.last_blend_src);
        glGetIntegerv(GL_BLEND_DST, &backup.last_blend_dst);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &backup.last_blend_equation_rgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &backup.last_blend_equation_alpha);
        glGetIntegerv(GL_VIEWPORT, backup.last_viewport);
        glGetIntegerv(GL_SCISSOR_BOX, backup.last_scissor);
        backup.last_enable_blend = glIsEnabled(GL_BLEND);
        backup.last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
        backup.last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
        backup.last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    }

    void ogl_graphics_driver::load_gl_state() {
        glUseProgram(backup.last_program);
        glActiveTexture(static_cast<GLenum>(backup.last_active_texture));
        glBindTexture(GL_TEXTURE_2D, backup.last_texture);
        glBindVertexArray(backup.last_vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, backup.last_array_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backup.last_element_array_buffer);

        glBlendEquationSeparate(static_cast<GLenum>(backup.last_blend_equation_rgb),
            static_cast<GLenum>(backup.last_blend_equation_alpha));

        glBlendFunc(static_cast<GLenum>(backup.last_blend_src),
            static_cast<GLenum>(backup.last_blend_dst));

        if (backup.last_enable_blend == GL_TRUE) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }

        if (backup.last_enable_cull_face == GL_TRUE) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }

        if (backup.last_enable_depth_test == GL_TRUE) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        if (backup.last_enable_scissor_test == GL_TRUE) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        glViewport(backup.last_viewport[0],
            backup.last_viewport[1],
            static_cast<GLsizei>(backup.last_viewport[2]),
            static_cast<GLsizei>(backup.last_viewport[3]));

        glScissor(backup.last_scissor[0],
            backup.last_scissor[1],
            backup.last_scissor[2],
            backup.last_scissor[3]);
    }

    std::unique_ptr<graphics_command_list> ogl_graphics_driver::new_command_list() {
        return std::make_unique<server_graphics_command_list>();
    }

    std::unique_ptr<graphics_command_list_builder> ogl_graphics_driver::new_command_builder(graphics_command_list *list) {
        return std::make_unique<server_graphics_command_list_builder>(list);
    }

    void ogl_graphics_driver::submit_command_list(graphics_command_list &command_list) {
        list_queue.push(static_cast<server_graphics_command_list &>(command_list));
    }

    void ogl_graphics_driver::display(command_helper &helper) {
        disp_hook_();
        helper.finish(this, 0);
    }

    void ogl_graphics_driver::dispatch(command *cmd) {
        command_helper helper(cmd);

        switch (cmd->opcode_) {
        case graphics_driver_draw_bitmap: {
            draw_bitmap(helper);
            break;
        }

        case graphics_driver_set_invalidate: {
            set_invalidate(helper);
            break;
        }

        case graphics_driver_invalidate_rect: {
            invalidate_rect(helper);
            break;
        }

        case graphics_driver_draw_indexed: {
            draw_indexed(helper);
            break;
        }

        case graphics_driver_backup_state: {
            save_gl_state();
            break;
        }

        case graphics_driver_restore_state: {
            load_gl_state();
            break;
        }

        case graphics_driver_blend_formula: {
            blend_formula(helper);
            break;
        }

        case graphics_driver_set_depth: {
            set_depth(helper);
            break;
        }

        case graphics_driver_set_blend: {
            set_blend(helper);
            break;
        }

        case graphics_driver_set_cull: {
            set_cull(helper);
            break;
        }

        case graphics_driver_clear: {
            clear(helper);
            break;
        }

        case graphics_driver_set_viewport: {
            set_viewport(helper);
            break;
        }

        case graphics_driver_display: {
            display(helper);
            break;
        }

        case graphics_driver_draw_rectangle: {
            draw_rectangle(helper);
            break;
        }

        default:
            shared_graphics_driver::dispatch(cmd);
            break;
        }
    }

    void ogl_graphics_driver::run() {
        while (!should_stop) {
            std::optional<server_graphics_command_list> list = list_queue.pop();

            if (!list) {
                LOG_ERROR("Corrupted graphics command list! Emulation halt.");
                break;
            }

            command *cmd = list->list_.first_;
            command *next = nullptr;

            while (cmd) {
                dispatch(cmd);
                next = cmd->next_;

                // TODO: If any command list requires not rebuilding the buffer, dont delete the command.
                // For now, not likely
                delete cmd;
                cmd = next;
            }
        }
    }

    void ogl_graphics_driver::abort() {
        list_queue.abort();
        should_stop = true;
    }
}
