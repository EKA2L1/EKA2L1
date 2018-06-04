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

#include <epoc9/register.h>

void register_epoc9(eka2l1::hle::lib_manager& mngr) {
    ADD_REGISTERS(mngr, thread_register_funcs);
    ADD_REGISTERS(mngr, base_register_funcs);
    ADD_REGISTERS(mngr, mem_register_funcs);
    ADD_REGISTERS(mngr, char_register_funcs);
    ADD_REGISTERS(mngr, allocator_register_funcs);
    ADD_REGISTERS(mngr, user_register_funcs);
    ADD_REGISTERS(mngr, hal_register_funcs);
}