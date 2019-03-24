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

#include <epoc/services/cdl/common.h>
#include <epoc/services/cdl/watcher.h>
#include <epoc/services/ecom/ecom.h>

#include <common/algorithm.h>
#include <common/cvt.h>

#include <fmt/format.h>

namespace eka2l1::epoc {
    void cdl_ecom_watcher::refresh_plugin_list() {
        ecom_interface_info *interface_info = ecom_->get_interface(cdl_uid);
        curr = &interface_info->implementations;

        common::addition_callback_func add_callback = 
            [&](const std::size_t idx) { 
                observer_->entry_added(std::u16string(1, drive_to_char16(curr->at(idx).drv)) 
                    + u":\\" + common::utf8_to_ucs2(common::to_string(curr->at(idx).uid, std::hex)) + u".dll");
            };

        common::remove_callback_func rev_callback = 
            [&](const std::size_t idx) { 
                observer_->entry_removed(std::u16string(1, drive_to_char16(last[idx].drv)) 
                    + u":\\" + common::utf8_to_ucs2(common::to_string(last[idx].uid, std::hex)) + u".dll");
            };

        common::compare_func<ecom_implementation_info> comp = 
            [](const ecom_implementation_info &lhs, const ecom_implementation_info &rhs) {
                if (lhs.uid < rhs.uid) {
                    return -1;
                } else if (lhs.uid == rhs.uid) {
                    return 0;
                }

                return 1;
            };

        common::detect_changes(last, *curr, add_callback, rev_callback, comp);

        // Make a copy
        last = *curr;
    }
}
