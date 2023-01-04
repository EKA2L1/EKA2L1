/*
 * Copyright (c) 2023 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#pragma once

// This include is for implementation file to use. Do not delete
#include <dispatch/libraries/vg/gnuVG_graphicstate.hh>
#include <dispatch/def.h>

namespace eka2l1 {
    class system;
}

namespace gnuVG {
    class Context;

    Context *get_active_context(eka2l1::system *sys, GraphicState *graphic_state = nullptr);
}