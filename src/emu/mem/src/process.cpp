/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <mem/model/flexible/process.h>
#include <mem/model/multiple/process.h>
#include <mem/process.h>

namespace eka2l1::mem {
    mem_model_process_impl make_new_mem_model_process(mmu_base *mmu, const mem_model_type model) {
        switch (model) {
        case mem_model_type::multiple: {
            return std::make_unique<multiple_mem_model_process>(mmu);
        }

        case mem_model_type::flexible:
            return std::make_unique<flexible::flexible_mem_model_process>(mmu);

        default:
            break;
        }

        return nullptr;
    }
}
