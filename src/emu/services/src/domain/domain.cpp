/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <services/domain/domain.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    dm_domain_server::dm_domain_server(eka2l1::system *sys)
        : service::typical_server(sys, "!DmDomainServer") {
    }

    void dm_domain_server::connect(service::ipc_context &context) {
        create_session<dm_domain_session>(&context);
        context.complete(epoc::error_none);
    }

    dm_domain_session::dm_domain_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void dm_domain_session::join(service::ipc_context *ctx) {
        std::optional<std::int32_t> hierarchy_id = ctx->get_argument_value<std::int32_t>(0);
        std::optional<std::int32_t> domain_id = ctx->get_argument_value<std::int32_t>(1);

        if (!hierarchy_id.has_value() || !domain_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        // The client attaches to this property right after joining and panics if reading it fails,
        // so it has to exist before the join completes.
        kernel_system *kern = ctx->sys->get_kernel_system();
        const std::int32_t key = epoc::dm::state_property_key(static_cast<std::uint32_t>(hierarchy_id.value()),
            static_cast<std::uint32_t>(domain_id.value()));

        property_ptr state_prop = kern->get_prop(static_cast<int>(epoc::dm::PROPERTY_CATEGORY), key);

        if (!state_prop) {
            state_prop = kern->create<service::property>();
            state_prop->first = static_cast<int>(epoc::dm::PROPERTY_CATEGORY);
            state_prop->second = key;

            state_prop->define(service::property_type::int_data, 4);
            state_prop->set_int(epoc::dm::state_property_value(0, epoc::dm::POWER_STATE_ACTIVE));
        }

        ctx->complete(epoc::error_none);
    }

    void dm_domain_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::dm::member_domain_join:
            join(ctx);
            break;

        // No transition ever happens, so there is nothing to acknowledge, notify or cancel. The
        // notification itself is delivered through the state property, which never changes.
        case epoc::dm::member_state_acknowledge:
        case epoc::dm::member_state_request_transition_notification:
        case epoc::dm::member_state_cancel_transition_notification:
        case epoc::dm::member_state_cancel_deferral:
            ctx->complete(epoc::error_none);
            break;

        // Deferring is only legal while a transition is in flight, which never is the case here.
        case epoc::dm::member_state_defer_acknowledgement:
            ctx->complete(epoc::error_not_ready);
            break;

        default:
            LOG_ERROR(SERVICE_DOMAIN, "Unimplemented domain member server opcode {}", ctx->msg->function);
            ctx->complete(epoc::error_not_supported);
            break;
        }
    }
}
