/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <common/container.h>
#include <services/framework.h>

#include <memory>

namespace eka2l1 {
    class accessory_server : public service::typical_server {
    public:
        explicit accessory_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct accessory_subsession {
    protected:
        accessory_server *serv_;

    public:
        explicit accessory_subsession(accessory_server *svr);
        virtual bool fetch(service::ipc_context *ctx) = 0;
    };

    struct accessory_single_connection_subsession : public accessory_subsession {
    protected:
        epoc::notify_info accessory_connected_nof_;

    public:
        explicit accessory_single_connection_subsession(accessory_server *svr);

        bool fetch(service::ipc_context *ctx) override;
        void notify_new_accessory_connected(service::ipc_context *ctx);
        void cancel_notify_new_accessory_connected(service::ipc_context *ctx);
        void get_accessory_connection_status(service::ipc_context *ctx);
    };

    using accessory_subsession_instance = std::unique_ptr<accessory_subsession>;

    struct accessory_session : public service::typical_session {
    private:
        common::identity_container<accessory_subsession_instance> subsessions_;

    public:
        explicit accessory_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void create_accessory_single_connection_subsession(service::ipc_context *ctx);
    };
}
