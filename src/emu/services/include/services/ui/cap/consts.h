/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <cstdint>

namespace eka2l1::epoc {
    static constexpr std::uint32_t FEP_FRAMEWORK_REPO_UID = 0x10272618;
    static constexpr std::uint32_t FEP_RESOURCE_ID = 0x008;     /*19BD6008*/

    enum fep_framework_repo_key {
        fep_framework_repo_keymask_default_setting = 0x1000,
        fep_framework_repo_keymask_dynamic_setting = 0x2000,

        fep_framework_repo_keymask_fepid = 0x1,
        fep_framework_repo_keymask_on_state = 0x2,
        fep_framework_repo_keymask_on_key_data = 0x4,
        fep_framework_repo_keymask_off_key_data = 0x8,

        fep_framework_repo_key_default_fepid = fep_framework_repo_keymask_default_setting | fep_framework_repo_keymask_fepid,
        fep_framework_repo_key_default_on_state = fep_framework_repo_keymask_default_setting | fep_framework_repo_keymask_on_state,
        fep_framework_repo_key_default_on_key_data = fep_framework_repo_keymask_default_setting| fep_framework_repo_keymask_on_key_data,
        fep_framework_repo_key_default_off_key_data = fep_framework_repo_keymask_default_setting | fep_framework_repo_keymask_off_key_data,

        fep_framework_repo_key_dynamic_fepid = fep_framework_repo_keymask_dynamic_setting | fep_framework_repo_keymask_fepid,
        fep_framework_repo_key_dynamic_on_state = fep_framework_repo_keymask_dynamic_setting | fep_framework_repo_keymask_on_state,
        fep_framework_repo_key_dynamic_on_key_data = fep_framework_repo_keymask_dynamic_setting | fep_framework_repo_keymask_on_key_data,
        fep_framework_repo_key_dynamic_off_key_data = fep_framework_repo_keymask_dynamic_setting | fep_framework_repo_keymask_off_key_data
    };
}