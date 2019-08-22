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

#include <drivers/graphics/graphics.h>
#include <drivers/graphics/imgui_renderer.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace eka2l1::drivers {
    static const char *gl_vertex_shader_renderer = "#version 330\n"
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

    static const char *gl_fragment_shader_renderer = "#version 330\n"
                                                     "uniform sampler2D Texture;\n"
                                                     "in vec2 Frag_UV;\n"
                                                     "in vec4 Frag_Color;\n"
                                                     "out vec4 Out_Color;\n"
                                                     "void main()\n"
                                                     "{\n"
                                                     "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
                                                     "}\n";

    static void get_render_shader(graphics_driver *driver, const char *&vert_dat, const char *&frag_dat) {
        switch (driver->get_current_api()) {
        case graphic_api::opengl: {
            vert_dat = gl_vertex_shader_renderer;
            frag_dat = gl_fragment_shader_renderer;

            break;
        }

        default:
            break;
        }
    }

    void imgui_renderer::init(graphics_driver *driver) {
        const char *vert_dat = nullptr;
        const char *frag_dat = nullptr;

        get_render_shader(driver, vert_dat, frag_dat);
        shader = create_program(driver, vert_dat, 0, frag_dat, 0);
    }
}
