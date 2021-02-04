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

namespace eka2l1 {
    struct socket_client_session;

    namespace service {
        struct ipc_context;
    };
}

namespace eka2l1::epoc::socket {
    struct saddress {
        std::uint8_t dat_[32];
    };

    static constexpr std::uint32_t SOCKET_OPTION_FAMILY_BASE = 1;
    static constexpr std::uint32_t SOCKET_OPTION_ID_BLOCKING_IO = 5;

    enum socket_subsession_type {
        socket_subsession_type_host_resolver,
        socket_subsession_type_connection,
        socket_subsession_type_socket
    };

    class socket_subsession {
    protected:
        socket_client_session *parent_;
        std::uint32_t id_;

    public:
        explicit socket_subsession(socket_client_session *parent)
            : parent_(parent) {
        }

        void set_id(const std::uint32_t new_id) {
            id_ = new_id;
        }

        virtual ~socket_subsession() {}

        virtual void dispatch(service::ipc_context *ctx) = 0;
        virtual socket_subsession_type type() const = 0;
    };
}