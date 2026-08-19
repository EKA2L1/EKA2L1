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

#pragma once

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {
    namespace epoc::dm {
        // Category of the properties the domain manager publishes each domain's state through,
        // and the key/value packing the member client (domaincli.dll) expects. See domainsrv.h.
        static constexpr std::uint32_t PROPERTY_CATEGORY = 0x1020E406;

        static constexpr std::int32_t state_property_key(const std::uint32_t hierarchy_id,
            const std::uint32_t domain_id) {
            return static_cast<std::int32_t>((hierarchy_id << 8) | ((domain_id << 8) & 0xFF0000) | (domain_id & 0xFF));
        }

        static constexpr std::int32_t state_property_value(const std::uint32_t id, const std::uint32_t state) {
            return static_cast<std::int32_t>((id << 24) | (state & 0xFFFFFF));
        }

        // TPowerState::EPwActive, the steady state of a power-hierarchy domain.
        static constexpr std::uint32_t POWER_STATE_ACTIVE = 0;

        enum member_opcode {
            member_domain_join = 0,
            member_state_acknowledge = 1,
            member_state_request_transition_notification = 2,
            member_state_cancel_transition_notification = 3,
            member_state_defer_acknowledgement = 4,
            member_state_cancel_deferral = 5
        };
    }

    // Member side of the domain manager (!DmDomainServer). Clients join a domain of a hierarchy,
    // then read its state from a publish-and-subscribe property and subscribe for transitions.
    //
    // The emulator has no power or startup state machine, so a domain never transitions: joining
    // just publishes the active state and notification requests are accepted but never fire. Apps
    // that merely observe the domain (Symbian^3 File manager does, and exits when the connection
    // fails) can then start up.
    class dm_domain_server : public service::typical_server {
    public:
        explicit dm_domain_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };

    struct dm_domain_session : public service::typical_session {
        explicit dm_domain_session(service::typical_server *serv, const kernel::uid ss_id,
            epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;

    private:
        void join(service::ipc_context *ctx);
    };
}
