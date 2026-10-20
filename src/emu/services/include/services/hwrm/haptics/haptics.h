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

#include <drivers/hwrm/vibration.h>
#include <services/framework.h>
#include <services/hwrm/haptics/protocol.h>

namespace eka2l1 {
    class haptics_session;

    class haptics_server : public service::typical_server {
        std::unique_ptr<drivers::hwrm::vibrator> vibrator_;
        haptics_session *output_owner_ = nullptr;
        std::vector<haptics_session *> reservations_;
        void notify_reservations();
        void shutdown();

    public:
        explicit haptics_server(system *sys);
        ~haptics_server() override;
        int destroy() override;
        void connect(service::ipc_context &ctx) override;
        void update_output(haptics_session *session, std::uint64_t now);
        void detach(haptics_session *session);
        void reserve(haptics_session *session);
        void release(haptics_session *session);
        std::int32_t status(haptics_session *session) const;
    };

    class haptics_session : public service::typical_session {
        friend class haptics_server;
        epoc::haptics::device device_;
        bool initialized_ = false;
        bool status_changed_ = false;
        bool actuator_changed_ = false;
        std::int32_t last_status_ = 0;
        std::int32_t actuator_ = 0;
        std::unique_ptr<service::ipc_context> status_request_;

        void cleanup();
        void notify_status();
        void execute(service::ipc_context *ctx);

    public:
        haptics_session(service::typical_server *serv, kernel::uid id, epoc::version version);
        ~haptics_session() override;
        void fetch(service::ipc_context *ctx) override;
    };
}
