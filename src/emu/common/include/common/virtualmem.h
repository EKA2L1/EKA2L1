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
    /**
     * \brief Map memory with defined size.
     *
     * \returns A valid pointer on success. Nullptr is fail.
    */
    void *map_memory(const std::size_t size);

    /**
     * \brief Unmap an pointer which points to a mapped region
     *
     * \param ptr The pointer poiting to target region
     * \param size Size to unmap
     * 
     * \returns False on failure, true on success.
     *          Failure may happens because the pointer are not mapped yet.
    */
    bool unmap_memory(void *ptr, const std::size_t size);

    /**
     * \brief Commit reserved memory region.
     *
     * \param ptr Pointer to the target region.
     * \param size Size of the memory to be committed.
     * \param prot The initial protection.
     * 
     * \returns True on success, false on failure.
    */
    bool commit(void *ptr, const std::size_t size,
        const prot commit_prot);

    /**
     * \brief Decommit memory region.
     *
     * \param ptr Pointer to the target region.
     * \param size Size of the memory to be decommitted.
     * 
     * \returns True on success, false on failure.
    */
    bool decommit(void *ptr, const std::size_t size);

    /**
     * \brief Change protection of committed region
     *
     * \param ptr Pointer to the target region.
     * \param size Size of the memory to be change.
     * \param prot The new protection.
     * 
     * \returns True on success, false on failure.
    */
    bool change_protection(void *ptr, const std::size_t size,
        const prot new_prot);

    /**
     * \brief Get the host's page size
    */
    int get_host_page_size();

    /**
     * \brief Map a file to memory with read-only attribute.
     *
     * \returns A valid pointer to the mapped region on success.
    */
    void *map_file(const std::string &file_name, const prot perm = prot::read,
        const std::size_t size = 0);

    /**
     * \brief Unmap a file mapped to memory
     *
     * \returns True on success.
    */
    bool unmap_file(void *ptr);

    /**
     * \brief Returns true if the platform doesn't allow write and executable memory at the same time.
    */
    bool is_memory_wx_exclusive();
}
