/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <epoc/services/cdl/hleutils.h>

namespace eka2l1::epoc {
    cdl_customisation *get_customisation(kernel::process *pr, cdl_main *main, const std::int32_t cust_index) {
        cdl_array *arr = main->data_.cast<cdl_array>().get(pr);

        if (!arr) {
            return nullptr;
        }

        // Out of range, then die
        if (cust_index >= arr->count_) {
            return nullptr;
        }

        return arr->data_.cast<cdl_customisation>().get(pr) + cust_index;
    }

    void *get_data_from_customisation(kernel::process *pr, cdl_customisation *cust, const std::int32_t data_index) {
        return reinterpret_cast<void **>(cust->impl_.get(pr))[data_index];
    }
}