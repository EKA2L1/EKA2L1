/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <utils/sec.h>
#include <cstdint>

namespace eka2l1::epoc::fs {
    static epoc::security_policy private_comp_access_policy({ epoc::cap_all_files });
    static epoc::security_policy sys_read_only_access_policy({ epoc::cap_all_files });
    static epoc::security_policy resource_read_only_access_policy{};
    static epoc::security_policy sys_resource_modify_access_policy({ epoc::cap_tcb });
}