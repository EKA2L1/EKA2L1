/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <utils/guest/actsched.h>
#include <common/log.h>

namespace eka2l1::utils {
    void active_object::dump(kernel::process *owner) {;
        LOG_INFO(UTILS, "\t-Code: {}", sts_.status);

        std::string flag_string;

        if (sts_.flags) {
            if (sts_.flags & epoc::request_status::active) {
                flag_string += "Active";
            }

            if (sts_.flags & epoc::request_status::pending) {
                flag_string += (flag_string.empty()) ? "Pending" : ", Pending";
            }
        }

        if (flag_string.empty())
            flag_string = "Not active";

        LOG_INFO(UTILS, "\t-Flags: {} ({})", flag_string, sts_.flags);
        LOG_INFO(UTILS, "-----------------------------------");
    }
    
    void active_scheduler::dump(kernel::process *owner) {
        LOG_INFO(UTILS, "Active scheduler dump");
        LOG_INFO(UTILS, "(note: Address of active object = address of request status)");
        LOG_INFO(UTILS, "-----------------------------------");

        eka2l1::ptr<double_queue_link> link = act_queue_.head_.next_;

        do {
            double_queue_link *real_link = link.get(owner);

            // Get the object that own the link
            if (!real_link) {
                break;
            }

            eka2l1::ptr<active_object> act_obj_addr = (link + (-act_queue_.offset_to_link_)).cast<active_object>();
            LOG_TRACE(UTILS, "Active object 0x{:X}", act_obj_addr.ptr_address());

            active_object *obj = act_obj_addr.get(owner);

            if (!obj) {
                LOG_INFO(UTILS, "\tInvalid!");
                LOG_INFO(UTILS, "-----------------------------------");
                break;
            }

            obj->dump(owner);

            if (link == act_queue_.head_.prev_) {
                // We reached a full circle
                break;
            }

            link = real_link->next_;
        } while (true);
    }
}