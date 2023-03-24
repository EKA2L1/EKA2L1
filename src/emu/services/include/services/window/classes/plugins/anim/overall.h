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

#pragma once

#include <services/window/classes/plugins/animdll.h>

#include <unordered_map>
#include <string>
#include <memory>

namespace eka2l1::epoc {
    using anim_dll_filename_and_factory_list = std::vector<std::pair<std::u16string, std::unique_ptr<anim_executor_factory>>>;
    void add_anim_executor_factory_to_list(anim_dll_filename_and_factory_list &factories);
}