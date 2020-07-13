/*
 * Copyright (c) 2020 EKA2L1 Team
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

#pragma once

#include <services/framework.h>
#include <kernel/server.h>

namespace eka2l1 {

    enum drm_notifier_opcode {
        notifier_notify_clients = 0x1,
        notifier_receive_notification = 0x2,
        notifier_register = 0x3,
        notifier_unregister = 0x4,
        notifier_register_uri = 0x5,
        notifier_unregister_uri = 0x6,
        notifier_cancel_notification = 0xFF
    };

    class drm_notifier_server : public service::typical_server {
    public:
        explicit drm_notifier_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct drm_notifier_client_session : public service::typical_session {
        explicit drm_notifier_client_session(service::typical_server *serv, const std::uint32_t ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
    };
}
