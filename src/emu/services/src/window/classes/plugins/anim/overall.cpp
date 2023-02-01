/*
 * Copyright (c) 2023 EKA2L1 Team
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

#include <services/window/classes/plugins/anim/overall.h>
#include <services/window/classes/plugins/anim/clock/factory.h>

namespace eka2l1::epoc {
    void add_anim_executor_factory_to_list(anim_dll_filename_and_factory_list &factories) {
        factories.emplace_back(u"Z:\\SYSTEM\\LIBS\\CLOCKA.DLL", std::make_unique<clock_anim_executor_factory>());
    }
}
    