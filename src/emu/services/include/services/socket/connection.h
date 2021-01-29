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
#include <common/container.h>

#include <cstdint>
#include <functional>
#include <string>
#include <memory>

namespace eka2l1 {
    class socket_server;
}

namespace eka2l1::epoc::socket {
    struct protocol;
    struct socket;

    struct conn_preferences {
        std::uint32_t reserved_;
    };

    struct conn_progress {
        std::int32_t stage_;
        std::int32_t error_;
    };

    using progress_advance_callback = std::function<void(conn_progress*)>;

    enum setting_type {
        setting_type_bool,
        setting_type_int,
        setting_type_des
    };

    struct connection {
    protected:
        common::identity_container<progress_advance_callback> progress_callbacks_;

        protocol *pr_;          ///< Connection protocol.
        socket *sock_;          ///< Connect requester.
        saddress dest_;          ///< The target address to connect to.

    public:
        explicit connection(protocol *pr, saddress dest);

        std::size_t register_progress_advance_callback(progress_advance_callback cb);
        bool remove_progress_advance_callback(const std::size_t handle);

        void set_source_socket(socket *sock) {
            sock_ = sock;
        }

        /**
         * @brief Get connection setting.
         * 
         * @param setting_name      Lookup string in form of CommsDB <table_name>\<column_name>. Invalid form will fail.
         * @param type              The type of setting to get.
         * @param dest_buffer       Destination buffer holding setting data.
         * @param avail_size        Maximum data destination buffer can hold.
         * 
         * @returns (size_t)(-1) on failure, else the written size to the buffer.
         */
        virtual std::size_t get_setting(const std::u16string &setting_name, const setting_type type, std::uint8_t *dest_buffer,
            std::size_t avail_size) = 0;
    };

    /**
     * @brief Plug-in that helps choosing a connection. This includes a target protocol and target address.
     * 
     * For example, in Symbian, there may be dialog which let you choose to either access GPRS or WLAN, and
     * choose the internet access point you desired. This is all done by connect agent.
     */
    struct connect_agent {
    protected:
        socket_server *sock_serv_;

    public:
        explicit connect_agent(socket_server *serv)
            : sock_serv_(serv) {
        }

        virtual std::u16string agent_name() const = 0;

        /**
         * @brief       Initiatiate new connection, which protocols are choosen through agent-based methods.
         * 
         * @param       prefs       Options for the agent to consider when establishing new connection.
         * @returns     New connection instance on success. Instantiate from protocol factory method.
         */
        virtual std::unique_ptr<connection> start_connection(conn_preferences &prefs) = 0;
    };
}