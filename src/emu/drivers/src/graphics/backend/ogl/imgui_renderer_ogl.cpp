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

#include <drivers/graphics/backend/ogl/imgui_renderer_ogl.h>
#include <glad/glad.h>
#include <imgui.h>

#include <common/log.h>

namespace eka2l1::drivers {
    struct gl_state {
        GLint last_program;
        GLint last_texture;
        GLint last_active_texture;
        GLint last_array_buffer;
        GLint last_element_array_buffer;
        GLint last_vertex_array;
        GLint last_blend_src;
        GLint last_blend_dst;
        GLint last_blend_equation_rgb;
        GLint last_blend_equation_alpha;
        GLint last_viewport[4];
        GLint last_scissor[4];
        GLboolean last_enable_blend;
        GLboolean last_enable_cull_face;
        GLboolean last_enable_depth_test;
        GLboolean last_enable_scissor_test;
    };

    static void save_gl_state(gl_state &state) {
        glGetIntegerv(GL_CURRENT_PROGRAM, &state.last_program);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.last_texture);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &state.last_active_texture);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state.last_array_buffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &state.last_element_array_buffer);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.last_vertex_array);
        glGetIntegerv(GL_BLEND_SRC, &state.last_blend_src);
        glGetIntegerv(GL_BLEND_DST, &state.last_blend_dst);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &state.last_blend_equation_rgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state.last_blend_equation_alpha);
        glGetIntegerv(GL_VIEWPORT, state.last_viewport);
        glGetIntegerv(GL_SCISSOR_BOX, state.last_scissor);
        state.last_enable_blend = glIsEnabled(GL_BLEND);
        state.last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
        state.last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
        state.last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    }

    static void load_gl_state(gl_state &state) {
        glUseProgram(state.last_program);
        glActiveTexture(static_cast<GLenum>(state.last_active_texture));
        glBindTexture(GL_TEXTURE_2D, state.last_texture);
        glBindVertexArray(state.last_vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, state.last_array_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.last_element_array_buffer);

        glBlendEquationSeparate(static_cast<GLenum>(state.last_blend_equation_rgb),
            static_cast<GLenum>(state.last_blend_equation_alpha));

        glBlendFunc(static_cast<GLenum>(state.last_blend_src),
            static_cast<GLenum>(state.last_blend_dst));

        if (state.last_enable_blend == GL_TRUE) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }

        if (state.last_enable_cull_face == GL_TRUE) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }

        if (state.last_enable_depth_test == GL_TRUE) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        if (state.last_enable_scissor_test == GL_TRUE) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        glViewport(state.last_viewport[0],
            state.last_viewport[1],
            static_cast<GLsizei>(state.last_viewport[2]),
            static_cast<GLsizei>(state.last_viewport[3]));

        glScissor(state.last_scissor[0],
            state.last_scissor[1],
            state.last_scissor[2],
            state.last_scissor[3]);
    }

    static const GLchar *vertex_shader_renderer = "#version 330\n"
                                                  "uniform mat4 ProjMtx;\n"
                                                  "in vec2 Position;\n"
                                                  "in vec2 UV;\n"
                                                  "in vec4 Color;\n"
                                                  "out vec2 Frag_UV;\n"
                                                  "out vec4 Frag_Color;\n"
                                                  "void main()\n"
                                                  "{\n"
                                                  "	Frag_UV = UV;\n"
                                                  "	Frag_Color = Color;\n"
                                                  "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
                                                  "}\n";

    static const GLchar *fragment_shader_renderer = "#version 330\n"
                                                    "uniform sampler2D Texture;\n"
                                                    "in vec2 Frag_UV;\n"
                                                    "in vec4 Frag_Color;\n"
                                                    "out vec4 Out_Color;\n"
                                                    "void main()\n"
                                                    "{\n"
                                                    "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
                                                    "}\n";

    void ogl_imgui_renderer::init() {
        shader.create(vertex_shader_renderer, 0, fragment_shader_renderer, 0);

        //err = glGetError();

        attrib_loc_tex = *shader.get_uniform_location("Texture");
        attrib_loc_proj_matrix = *shader.get_uniform_location("ProjMtx");
        attrib_loc_pos = *shader.get_attrib_location("Position");
        attrib_loc_uv = *shader.get_attrib_location("UV");
        attrib_loc_color = *shader.get_attrib_location("Color");

        glGenBuffers(1, &vbo_handle);
        glGenBuffers(1, &elements_handle);

        glGenVertexArrays(1, &vao_handle);
        glBindVertexArray(vao_handle);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
        glEnableVertexAttribArray(attrib_loc_pos);
        glEnableVertexAttribArray(attrib_loc_uv);
        glEnableVertexAttribArray(attrib_loc_color);

#define OFFSETOF(TYPE, ELEMENT) ((size_t) & (((TYPE *)0)->ELEMENT))
        glVertexAttribPointer(attrib_loc_pos, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (void *)OFFSETOF(ImDrawVert, pos));
        glVertexAttribPointer(attrib_loc_uv, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (void *)OFFSETOF(ImDrawVert, uv));
        glVertexAttribPointer(attrib_loc_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (void *)OFFSETOF(ImDrawVert, col));
#undef OFFSETOF

        auto &io = ImGui::GetIO();
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        font_texture.create(2, 0, vec3(width, height, 0), drivers::texture_format::rgba,
            drivers::texture_format::rgba, drivers::texture_data_type::ubyte, pixels);
        font_texture.set_filter_minmag(true, drivers::filter_option::linear);
        font_texture.set_filter_minmag(false, drivers::filter_option::linear);

        io.Fonts->TexID = reinterpret_cast<void *>(
            static_cast<int64_t>(font_texture.texture_handle()));
    }

    static std::string gl_error_to_string(const int e) {
        switch (e) {
        case GL_INVALID_ENUM: {
            return "Invalid enum passed to a function!";
        }

        case GL_INVALID_VALUE: {
            return "Invalid value passed to a function!";
        }

        case GL_INVALID_OPERATION: {
            return "An invalid operation happened!";
        }

        case GL_STACK_OVERFLOW: {
            return "GPU stack overflowed!";
        }

        case GL_STACK_UNDERFLOW: {
            return "GPU stack underflowed!";
        }

        case GL_OUT_OF_MEMORY: {
            return "GPU can't allocate more memory";
        }

        case GL_INVALID_FRAMEBUFFER_OPERATION: {
            return "Invalid framebuffer operation happended!";
        }

        default:
            break;
        }

        return fmt::format("Unknown error happended! (code: {})", e);
    }

    void ogl_imgui_renderer::render(ImDrawData *draw_data) {
        auto &io = ImGui::GetIO();

        // Scale clip rects
        auto fb_width = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
        auto fb_height = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
        draw_data->ScaleClipRects(io.DisplayFramebufferScale);

        // Backup GL state
        auto state = gl_state{};
        save_gl_state(state);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_SCISSOR_TEST);
        glActiveTexture(GL_TEXTURE0);

        // Setup viewport, orthographic projection matrix
        glViewport(0, 0, static_cast<GLsizei>(fb_width), static_cast<GLsizei>(fb_height));
        const float ortho_projection[4][4] = {
            { 2.0f / io.DisplaySize.x, 0.0f, 0.0f, 0.0f },
            { 0.0f, 2.0f / -io.DisplaySize.y, 0.0f, 0.0f },
            { 0.0f, 0.0f, -1.0f, 0.0f },
            { -1.0f, 1.0f, 0.0f, 1.0f },
        };

        shader.use();
        glUniform1i(attrib_loc_tex, 0);
        glUniformMatrix4fv(attrib_loc_proj_matrix, 1, GL_FALSE, &ortho_projection[0][0]);
        glBindVertexArray(vao_handle);

        for (auto n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList *cmd_list = draw_data->CmdLists[n];
            const ImDrawIdx *idx_buf_offset = nullptr;

            glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
            glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(cmd_list->VtxBuffer.size()) * sizeof(ImDrawVert),
                reinterpret_cast<const GLvoid *>(&cmd_list->VtxBuffer.front()),
                GL_STREAM_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elements_handle);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(cmd_list->IdxBuffer.size()) * sizeof(ImDrawIdx), 
                reinterpret_cast<const GLvoid *>(&cmd_list->IdxBuffer.front()), GL_STREAM_DRAW);

            for (auto pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++) {
                if (pcmd->UserCallback) {
                    pcmd->UserCallback(cmd_list, pcmd);
                } else {
                    glBindTexture(GL_TEXTURE_2D, static_cast<const GLuint>(reinterpret_cast<std::ptrdiff_t>(pcmd->TextureId)));
                    glScissor(static_cast<int>(pcmd->ClipRect.x), static_cast<int>(fb_height - pcmd->ClipRect.w), 
                        static_cast<int>(pcmd->ClipRect.z - pcmd->ClipRect.x), 
                        static_cast<int>(pcmd->ClipRect.w - pcmd->ClipRect.y));
                    
                    glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buf_offset);

                    // Support for OpenGL < 4.3
                    GLenum err = 0;

                    err = glGetError();
                    while (err != GL_NO_ERROR) {
                        LOG_ERROR("Error in OpenGL operation: {}", gl_error_to_string(err));
                        err = glGetError();
                    }
                }

                idx_buf_offset += pcmd->ElemCount;
            }
        }

        // Restore modified GL state
        load_gl_state(state);
    }

    void ogl_imgui_renderer::deinit() {
        glDeleteVertexArrays(1, &vao_handle);

        glDeleteBuffers(1, &vbo_handle);
        glDeleteBuffers(1, &elements_handle);
    }
}
