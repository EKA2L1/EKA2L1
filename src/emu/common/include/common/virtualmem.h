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

#pragma once

#include <common/types.h>
#include <cstdint>

namespace eka2l1::common {
    void *map_memory(const std::size_t size);
    bool  unmap_memory(void *ptr, const std::size_t size);

    bool commit(void *ptr, const std::size_t size,
        const prot commit_prot);
        
    bool decommit(void *ptr, const std::size_t size);

    bool change_protection(void *ptr, const std::size_t size,
        const prot new_prot);

    int get_host_page_size();

    void *map_file(const std::string &file_name);
    bool unmap_file(void *ptr);
}