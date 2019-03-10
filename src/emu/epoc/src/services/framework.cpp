/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/framework.h>

namespace eka2l1::service {
    bool normal_object_container::remove(epoc::ref_count_object *obj) {
        auto res = std::lower_bound(objs.begin(), objs.end(), obj,
            [](const ref_count_object_heap_ptr &lhs, const epoc::ref_count_object *rhs) {
                return lhs->id < rhs->id;
            });

        if (res == objs.end()) {
            return false;
        }

        objs.erase(res);
        return true;
    }

    typical_server::typical_server(system *sys, const std::string name) 
        : server(sys, name, true, false) {
    }

    void typical_server::process_accepted_msg() {
        int res = receive(process_msg);

        if (res == -1) {
            return;
        }

        ipc_context context{ sys, process_msg };
        auto func = ipc_funcs.find(process_msg->function);

        if (func != ipc_funcs.end()) {
            func->second.wrapper(context);
            return;
        }

        auto ss_ite = sessions.find(process_msg->msg_session->unique_id());

        if (ss_ite == sessions.end()) {
            return;
        }

        ss_ite->second->fetch(&context);
    }
}
