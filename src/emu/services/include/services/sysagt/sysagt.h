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

#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include <services/framework.h>

#include <utils/consts.h>
#include <utils/des.h>
#include <utils/system.h>

namespace eka2l1 {
    namespace epoc {
        struct request_status;
    }

    static constexpr std::uint32_t SYSTEM_AGENT_PROPERTY_CATEGORY = epoc::SYS_CATEGORY;

    enum system_agent_opcode {
        system_agent_get_state = 0,
        system_agent_get_multiple_states = 1,
        system_agent_notify_on_event = 2,
        system_agent_notify_on_any_event = 3,
        system_agent_notify_on_cond = 4,
        system_agent_notify_event_cancel = 5
    };

    struct system_agent_notify_info {
        epoc::notify_info woke_target_;
        epoc::des8 *target_uid_;            ///< Descriptor to write UID that got notified.
        epoc::des8 *state_value_;           ///< Descriptor to write state value of UID on event.

        std::uint32_t uid_nof_;             ///< UID to notify. Unsigned -1 means any.

        explicit system_agent_notify_info()
            : target_uid_(nullptr)
            , state_value_(nullptr)
            , uid_nof_(0) {
        }
    };

    class system_agent_server : public service::typical_server {
    public:
        explicit system_agent_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct system_agent_session : public service::typical_session {
    protected:
        system_agent_notify_info info_;

    public:
        explicit system_agent_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void get_state(service::ipc_context *ctx);
        void notify_event(service::ipc_context *ctx, const bool any);

        void fetch(service::ipc_context *ctx) override;
    };
}
