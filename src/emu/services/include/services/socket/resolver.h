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

#include <services/socket/common.h>
#include <cstdint>
#include <string>

namespace eka2l1::epoc::socket {
    class socket_connection;
    struct host;
    struct connection;

    class socket_host_resolver: public socket_subsession {
        std::uint32_t addr_fam_;
        std::uint32_t protocol_;

        connection *conn_;

    protected:
        void get_host_name(service::ipc_context *ctx);
        void set_host_name(service::ipc_context *ctx);
        void close(service::ipc_context *ctx);

        epoc::socket::host &lookup_host();

    public:
        explicit socket_host_resolver(socket_client_session *parent, const std::uint32_t addr_fam,
            const std::uint32_t protocol, connection *conn = nullptr);

        void dispatch(service::ipc_context *ctx) override;

        socket_subsession_type type() const override {
            return socket_subsession_type_host_resolver;
        }

        std::u16string host_name();
        void set_host_name(const std::u16string &new_name);
    };
}