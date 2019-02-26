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

#include <debugger/renderer/opengl/gl_renderer.h>
#include <debugger/renderer/renderer.h>

namespace eka2l1 {
    void debugger_renderer::init(drivers::graphics_driver_ptr graphic_driver, drivers::input_driver_ptr input_driver, debugger_ptr dbg) {
        debugger = dbg;

        gr_driver_ = std::move(graphic_driver);
        inp_driver_ = std::move(input_driver);
    }

    debugger_renderer_ptr new_debugger_renderer(const debugger_renderer_type rtype) {
        switch (rtype) {
        case debugger_renderer_type::opengl:
            return std::make_shared<debugger_gl_renderer>();

        default:
            break;
        }

        return nullptr;
    }
}
