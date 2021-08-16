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

#include <common/log.h>
#include <system/epoc.h>

#include <services/framework.h>
#include <services/utils.h>

namespace eka2l1::service {
    normal_object_container::~normal_object_container() {
        clear();
    }

    void normal_object_container::clear() {
        const std::lock_guard<std::mutex> guard(obj_lock);
        objs.clear();
    }

    bool normal_object_container::remove(epoc::ref_count_object *obj) {
        const std::lock_guard<std::mutex> guard(obj_lock);

        auto res = std::lower_bound(objs.begin(), objs.end(), obj,
            [](const ref_count_object_heap_ptr &lhs, const epoc::ref_count_object *rhs) {
                return lhs->id < rhs->id;
            });

        if (res == objs.end() || (res->get()->id != obj->id)) {
            return false;
        }

        // For sure, reset
        res->reset();

        objs.erase(res);
        return true;
    }

    typical_server::typical_server(system *sys, const std::string &name)
        : server(sys->get_kernel_system(), sys, nullptr, name, true, false) {
    }

    typical_server::~typical_server() {
        sessions.clear();
    }

    void typical_server::disconnect(service::ipc_context &ctx) {
        sessions.erase(ctx.msg->msg_session->unique_id());
        ctx.complete(0);
    }

    std::optional<epoc::version> typical_server::get_version(service::ipc_context *ctx) {
        kernel_system *kern = ctx->sys->get_kernel_system();
        std::optional<epoc::version> ver = get_server_version(kern, ctx);
        return ver;
    }

    void typical_server::process_accepted_msg() {
        ipc_msg_ptr process_msg = nullptr;
        receive(process_msg);

        if (!process_msg) {
            return;
        }

        ipc_context context;
        context.sys = sys;
        context.msg = process_msg;

        auto func = ipc_funcs.find(process_msg->function);

        if (func != ipc_funcs.end()) {
            func->second.wrapper(context);
            return;
        }

        auto ss_ite = sessions.find(process_msg->msg_session->unique_id());

        if (ss_ite == sessions.end()) {
            LOG_TRACE(SERVICE_TRACK, "Can't find responsible server-side session to client session with ID {}",
                process_msg->msg_session->unique_id());

            return;
        }

        ss_ite->second->fetch(&context);
    }
}
