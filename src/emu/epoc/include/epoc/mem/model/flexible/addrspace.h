/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <epoc/mem/model/section.h>
#include <epoc/mem/common.h>
#include <epoc/mem/page.h>

#include <memory>
#include <vector>

namespace eka2l1::mem {
}

namespace eka2l1::mem::flexible {
    struct mapping;
    struct mmu_flexible;

    struct address_space {
        page_directory *dir_;
        std::vector<mapping*> mappings_;

        mmu_flexible *mmu_;

        linear_section local_data_sec_;
        linear_section shared_data_sec_;
        linear_section ram_code_sec_;
        linear_section dll_static_data_sec_;

    public:
        explicit address_space(mmu_flexible *mmu);

        /**
         * @brief   Get the ID of this address space.
         * @returns The ID of this address space.
         */
        const asid id() const;

        /**
         * @brief   Get memory section specified by the flags.
         * 
         * @param   flags       Flag specify which section to get.
         * @returns Pointer to the section on success.
         */
        linear_section *section(const std::uint32_t flags);
    };
}