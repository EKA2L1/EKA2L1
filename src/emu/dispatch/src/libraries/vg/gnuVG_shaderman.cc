/*
 * gnuVG - a free Vector Graphics library
 * Copyright (C) 2016 by Anton Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <dispatch/libraries/vg/gnuVG_shaderman.hh>
#include <dispatch/libraries/vg/gnuVG_shader.hh>

namespace gnuVG {
	ShaderMan::~ShaderMan() {
		for (auto &[handle, shader]: shader_library) {
			delete shader;
		}
	}

    Shader* ShaderMan::get_shader(eka2l1::drivers::graphics_driver *drv, int caps) {
		auto found = shader_library.find(caps);
		if(found != shader_library.end()) {
			return found->second;
		}

		auto new_shader = new Shader(drv, caps);
		shader_library[caps] = new_shader;
		return new_shader;
    }
}