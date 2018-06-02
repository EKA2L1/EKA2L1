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
#include "../imguirdr.h"
#include <common/log.h>

#include <iostream>

namespace eka2l1 {
    namespace imgui {
        GLuint basic_shader_program;
        GLuint attrib_loc_tex;
        GLuint attrib_loc_proj_mat;
        GLuint attrib_loc_pos;
        GLuint attrib_loc_color;
        GLuint attrib_loc_uv;

        GLuint font_tex;

        GLuint vbo_handle;
        GLuint elements_handle;

        void free_gl() {
            glDeleteProgram(basic_shader_program);
        }

        void create_font_tx() {
            ImGuiIO &io = ImGui::GetIO();

            unsigned char *pixels;
            int width,
                height;

            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

            GLint last_texture;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
            glGenTextures(1, &font_tex);
            glBindTexture(GL_TEXTURE_2D, font_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

            io.Fonts->TexID = (void *)(intptr_t)(font_tex);

            // Restore state
            glBindTexture(GL_TEXTURE_2D, last_texture);
        }

        bool create_dvc_objs() {
            // Backup GL state
            GLint last_texture, last_array_buffer, last_vertex_array;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

            const GLchar *vertex_shader = "#version 150\n"
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

            const GLchar *fragment_shader = "#version 150\n"
                                            "uniform sampler2D Texture;\n"
                                            "in vec2 Frag_UV;\n"
                                            "in vec4 Frag_Color;\n"
                                            "out vec4 Out_Color;\n"
                                            "void main()\n"
                                            "{\n"
                                            "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
                                            "}\n";

            basic_shader_program = glCreateProgram();
            auto vertex_handle = glCreateShader(GL_VERTEX_SHADER);
            auto frag_handle = glCreateShader(GL_FRAGMENT_SHADER);

            glShaderSource(vertex_handle, 1, &vertex_shader, NULL);
            glShaderSource(frag_handle, 1, &fragment_shader, NULL);

            glCompileShader(vertex_handle);

            int success;
            char infoLog[512];

            glGetShaderiv(vertex_handle, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(vertex_handle, 512, NULL, infoLog);
                LOG_ERROR("Vertex shader error: {}", infoLog);
            };

            glCompileShader(frag_handle);

            glGetShaderiv(frag_handle, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(frag_handle, 512, NULL, infoLog);
                LOG_ERROR("Fragment shader error: {}", infoLog);
            };

            glAttachShader(basic_shader_program, vertex_handle);
            glAttachShader(basic_shader_program, frag_handle);
            glLinkProgram(basic_shader_program);

            glGetProgramiv(basic_shader_program, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(basic_shader_program, 512, NULL, infoLog);
                LOG_ERROR("Shader program link: {}", infoLog);
            }

            attrib_loc_tex = glGetUniformLocation(basic_shader_program, "Texture");
            attrib_loc_proj_mat = glGetUniformLocation(basic_shader_program, "ProjMtx");
            attrib_loc_pos = glGetAttribLocation(basic_shader_program, "Position");
            attrib_loc_uv = glGetAttribLocation(basic_shader_program, "UV");
            attrib_loc_color = glGetAttribLocation(basic_shader_program, "Color");

            glGenBuffers(1, &vbo_handle);
            glGenBuffers(1, &elements_handle);

            create_font_tx();

            // Restore modified GL state
            glBindTexture(GL_TEXTURE_2D, last_texture);
            glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
            glBindVertexArray(last_vertex_array);

            return true;
        }

        void draw_gl(ImDrawData *draw_data) {
            // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
            ImGuiIO &io = ImGui::GetIO();
            int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
            int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
            if (fb_width == 0 || fb_height == 0)
                return;
            draw_data->ScaleClipRects(io.DisplayFramebufferScale);

            // Backup GL state
            GLenum last_active_texture;
            glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *)&last_active_texture);
            glActiveTexture(GL_TEXTURE0);
            GLint last_program;
            glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
            GLint last_texture;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
            GLint last_sampler;
            glGetIntegerv(GL_SAMPLER_BINDING, &last_sampler);
            GLint last_array_buffer;
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
            GLint last_element_array_buffer;
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
            GLint last_vertex_array;
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
            GLint last_polygon_mode[2];
            glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
            GLint last_viewport[4];
            glGetIntegerv(GL_VIEWPORT, last_viewport);
            GLint last_scissor_box[4];
            glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
            GLenum last_blend_src_rgb;
            glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *)&last_blend_src_rgb);
            GLenum last_blend_dst_rgb;
            glGetIntegerv(GL_BLEND_DST_RGB, (GLint *)&last_blend_dst_rgb);
            GLenum last_blend_src_alpha;
            glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *)&last_blend_src_alpha);
            GLenum last_blend_dst_alpha;
            glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *)&last_blend_dst_alpha);
            GLenum last_blend_equation_rgb;
            glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *)&last_blend_equation_rgb);
            GLenum last_blend_equation_alpha;
            glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *)&last_blend_equation_alpha);
            GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
            GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
            GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
            GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

            // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_SCISSOR_TEST);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // Setup viewport, orthographic projection matrix
            glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
            const float ortho_projection[4][4] = {
                { 2.0f / io.DisplaySize.x, 0.0f, 0.0f, 0.0f },
                { 0.0f, 2.0f / -io.DisplaySize.y, 0.0f, 0.0f },
                { 0.0f, 0.0f, -1.0f, 0.0f },
                { -1.0f, 1.0f, 0.0f, 1.0f },
            };
            glUseProgram(basic_shader_program);
            glUniform1i(attrib_loc_tex, 0);
            glUniformMatrix4fv(attrib_loc_proj_mat, 1, GL_FALSE, &ortho_projection[0][0]);
            glBindSampler(0, 0); // Rely on combined texture/sampler state.

            // Recreate the VAO every time
            // (This is to easily allow multiple GL contexts. VAO are not shared among GL contexts, and we don't track creation/deletion of windows so we don't have an obvious key to use to cache them.)
            GLuint vao_handle = 0;
            glGenVertexArrays(1, &vao_handle);
            glBindVertexArray(vao_handle);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
            glEnableVertexAttribArray(attrib_loc_pos);
            glEnableVertexAttribArray(attrib_loc_uv);
            glEnableVertexAttribArray(attrib_loc_color);
            glVertexAttribPointer(attrib_loc_pos, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid *)IM_OFFSETOF(ImDrawVert, pos));
            glVertexAttribPointer(attrib_loc_uv, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid *)IM_OFFSETOF(ImDrawVert, uv));
            glVertexAttribPointer(attrib_loc_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid *)IM_OFFSETOF(ImDrawVert, col));

            // Draw
            for (int n = 0; n < draw_data->CmdListsCount; n++) {
                const ImDrawList *cmd_list = draw_data->CmdLists[n];
                const ImDrawIdx *idx_buffer_offset = 0;

                glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid *)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elements_handle);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid *)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

                for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                    const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
                    if (pcmd->UserCallback) {
                        pcmd->UserCallback(cmd_list, pcmd);
                    } else {
                        glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                        glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                        glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
                    }
                    idx_buffer_offset += pcmd->ElemCount;
                }
            }

            glDeleteVertexArrays(1, &vao_handle);

            // Restore modified GL state
            glUseProgram(last_program);
            glBindTexture(GL_TEXTURE_2D, last_texture);
            glBindSampler(0, last_sampler);
            glActiveTexture(last_active_texture);
            glBindVertexArray(last_vertex_array);
            glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
            glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
            glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
            if (last_enable_blend)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
            if (last_enable_cull_face)
                glEnable(GL_CULL_FACE);
            else
                glDisable(GL_CULL_FACE);
            if (last_enable_depth_test)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            if (last_enable_scissor_test)
                glEnable(GL_SCISSOR_TEST);
            else
                glDisable(GL_SCISSOR_TEST);
            glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
            glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
            glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
        }

        void newframe_gl(GLFWwindow *win) {
            if (!font_tex)
                create_dvc_objs();

            ImGuiIO &io = ImGui::GetIO();

            // Setup display size (every frame to accommodate for window resizing)
            int w, h;
            int display_w, display_h;
            glfwGetWindowSize(win, &w, &h);
            glfwGetFramebufferSize(win, &display_w, &display_h);
            io.DisplaySize = ImVec2((float)w, (float)h);
            io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

            ImGui::NewFrame();
        }

        void clear_gl(ImVec4 clcl) {
            glClearColor(clcl.x, clcl.y, clcl.z, clcl.w);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
}

