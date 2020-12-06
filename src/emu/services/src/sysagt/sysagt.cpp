/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <system/epoc.h>
#include <services/sysagt/sysagt.h>

#include <utils/consts.h>
#include <utils/err.h>

namespace eka2l1 {
    system_agent_server::system_agent_server(eka2l1::system *sys)
        : service::typical_server(sys, "SystemAgent") {
    }

    void system_agent_server::connect(service::ipc_context &context) {
        create_session<system_agent_session>(&context);
        context.complete(epoc::error_none);
    }

    bool system_agent_event_queue::listen(system_agent_notify_info &info) {
        if (!info_.woke_target_.empty()) {
            return false;
        }

        info_ = std::move(info);
        return true;
    }

    system_agent_session::system_agent_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void system_agent_session::get_state(service::ipc_context *ctx) {
        std::optional<epoc::uid> uid = ctx->get_argument_value<epoc::uid>(0);

        if (!uid) {
            ctx->complete(epoc::error_argument);
            return;
        }

        kernel_system *kern = ctx->sys->get_kernel_system();
        property_ptr prop = kern->get_prop(SYSTEM_AGENT_PROPERTY_CATEGORY, uid.value());

        if (!prop) {
            LOG_INFO("System Agent can't find state with UID 0x{:X}", uid.value());

            ctx->complete(epoc::error_not_found);
            return;
        }

        // Not check on type on purpose. TODO: This may be harmful.
        ctx->write_data_to_descriptor_argument<std::int32_t>(1, 0);
        ctx->complete(epoc::error_none);
    }

    void system_agent_session::get_multiple_states(service::ipc_context *ctx) {
        std::optional<std::uint32_t> count = ctx->get_argument_value<std::uint32_t>(0);

        if (!count.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::uint32_t size_expected = count.value() * sizeof(epoc::uid);
        const std::size_t size_of_sources = ctx->get_argument_max_data_size(1);
        const std::size_t size_of_dest = ctx->get_argument_max_data_size(2);

        std::uint32_t *sources = reinterpret_cast<std::uint32_t*>(ctx->
            get_descriptor_argument_ptr(1));

        std::uint32_t *dest = reinterpret_cast<std::uint32_t*>(ctx->
            get_descriptor_argument_ptr(2));

        if ((size_of_sources < size_expected) || (size_of_dest < size_expected)
            || !sources || !dest) {
            ctx->complete(epoc::error_argument);
            return;
        }

        kernel_system *kern = ctx->sys->get_kernel_system();

        for (std::uint32_t i = 0; i < count.value(); i++) {
            property_ptr prop = kern->get_prop(SYSTEM_AGENT_PROPERTY_CATEGORY,
                sources[i]);

            if (!prop) {
                LOG_INFO("System Agent can't find state with UID 0x{:X}, set result to 0", sources[i]);
                dest[i] = 0;
            } else {
                dest[i] = static_cast<std::uint32_t>(prop->get_int());
            }
        }

        ctx->set_descriptor_argument_length(2, size_expected);
        ctx->complete(epoc::error_none);
    }

    void system_agent_session::notify_event(service::ipc_context *ctx, const bool any) {
        std::optional<address> uid_des_addr = ctx->get_argument_value<address>(0);
        std::optional<address> state_des_addr = ctx->get_argument_value<address>(1);

        if (!uid_des_addr || !state_des_addr) {
            ctx->complete(epoc::error_argument);
            return;
        }

        kernel::process *own_pr = ctx->msg->own_thr->owning_process();

        epoc::des8 *uid_des = eka2l1::ptr<epoc::des8>(uid_des_addr.value()).get(own_pr);
        epoc::des8 *state_des = eka2l1::ptr<epoc::des8>(state_des_addr.value()).get(own_pr);

        if (!uid_des || !state_des) {
            ctx->complete(epoc::error_argument);
            return;
        }

        system_agent_notify_info info;

        if (any) {
            info.uid_nof_ = 0xFFFFFFFF;
        } else {
            std::uint32_t *uid_to_nof_ptr = reinterpret_cast<std::uint32_t*>(uid_des->get_pointer(own_pr));
            if (!uid_to_nof_ptr) {
                ctx->complete(epoc::error_bad_descriptor);
                return;
            }

            info.uid_nof_ = *uid_to_nof_ptr;
        }

        info.state_value_ = state_des;
        info.target_uid_ = uid_des;
        info.woke_target_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);

        if (!queue_.listen(info)) {
            ctx->complete(epoc::error_in_use);
            return;
        }
    }

    void system_agent_session::notify_event_cancel(service::ipc_context *ctx) {
        if (!queue_.info_.woke_target_.empty()) {
            queue_.info_.woke_target_.complete(epoc::error_cancel);
        }

        ctx->complete(epoc::error_none);
    }

    void system_agent_session::set_event_buffering(service::ipc_context *ctx) {
        std::optional<std::uint32_t> enabled = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<std::uint32_t> interval_in_secs = ctx->get_argument_value<std::uint32_t>(1);

        if (!enabled || !interval_in_secs) {
            ctx->complete(epoc::error_argument);
            return;
        }

        queue_.buffering_ = static_cast<bool>(enabled.value());
        queue_.time_expire_ = interval_in_secs.value();

        ctx->complete(epoc::error_none);
    }

    void system_agent_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case system_agent_get_state:
            get_state(ctx);
            break;

        case system_agent_get_multiple_states:
            get_multiple_states(ctx);
            break;

        case system_agent_notify_on_event:
            notify_event(ctx, false);
            break;

        case system_agent_notify_event_cancel:
            notify_event_cancel(ctx);
            break;

        case system_agent_set_event_buffer_enabled:
            set_event_buffering(ctx);
            break;
        
        default:
            LOG_ERROR("Unimplemented opcode for System Agent server 0x{:X}", ctx->msg->function);
            break;
        }
    }
}
