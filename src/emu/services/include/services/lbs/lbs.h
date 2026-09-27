/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <kernel/server.h>
#include <services/framework.h>

#include <cstdint>
#include <map>
#include <memory>

namespace eka2l1 {
    class kernel_system;
    class ntimer;

    struct lbs_update_options {
        std::int64_t interval_ = 0;
        std::int64_t timeout_ = 0;
        std::int64_t max_age_ = 0;
        bool accept_partial_ = false;
    };

    struct lbs_positioner {
        std::uint64_t id_;
        lbs_update_options options_;

        std::unique_ptr<service::ipc_context> update_msg_;
        bool update_times_out_ = false;

        std::uint64_t last_update_time_ = 0;
        bool has_updated_ = false;
    };

    /**
     * @brief Location server (PosServer, !PosServer from Symbian^3) exposing one GPS module that never gets a fix.
     *
     * Position requests complete like a GPS receiver searching for satellites: partial updates
     * carrying only a timestamp, or KErrTimedOut when the client does not accept them.
     */
    class lbs_server : public service::typical_server {
        kernel_system *kern_;
        ntimer *timing_;
        int update_evt_;

        std::map<std::uint64_t, lbs_positioner *> positioners_;
        std::uint64_t positioner_id_counter_ = 0;

        void complete_update(const std::uint64_t id);

    public:
        explicit lbs_server(eka2l1::system *sys);
        ~lbs_server() override;

        void connect(service::ipc_context &context) override;

        std::unique_ptr<lbs_positioner> new_positioner();
        void close_positioner(lbs_positioner *positioner);

        void schedule_update(lbs_positioner *positioner);
        void cancel_update(lbs_positioner *positioner, const int code);
    };

    struct lbs_client_session : public service::typical_session {
    private:
        std::map<std::uint32_t, std::unique_ptr<lbs_positioner>> positioners_;
        std::uint32_t handle_counter_ = 0;

        std::unique_ptr<service::ipc_context> status_event_msg_;

        lbs_positioner *get_positioner(service::ipc_context *ctx);

        void fetch_server(service::ipc_context *ctx, const std::uint32_t op);
        void fetch_positioner(service::ipc_context *ctx, const std::uint32_t op);

        void cancel_server_request(service::ipc_context *ctx);
        void get_default_module_id(service::ipc_context *ctx);
        void get_module_info(service::ipc_context *ctx, const bool by_index);
        void get_module_status(service::ipc_context *ctx);
        void notify_module_status_event(service::ipc_context *ctx);

        void open_positioner(service::ipc_context *ctx, const bool by_module_id);
        void close_positioner(service::ipc_context *ctx);
        void cancel_positioner_request(service::ipc_context *ctx);
        void set_update_options(service::ipc_context *ctx);
        void get_update_options(service::ipc_context *ctx);
        void notify_position_update(service::ipc_context *ctx);

    public:
        explicit lbs_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);
        ~lbs_client_session() override;

        void fetch(service::ipc_context *ctx) override;
    };
}
