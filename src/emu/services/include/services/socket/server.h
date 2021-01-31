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

#include <services/socket/common.h>
#include <services/socket/connection.h>
#include <services/socket/protocol.h>
#include <services/socket/host.h>

#include <services/framework.h>
#include <common/container.h>
#include <kernel/server.h>

#include <utils/des.h>

#include <map>
#include <memory>

namespace eka2l1 {
    namespace epoc::socket {
        class socket_connection_proxy: public socket_subsession {
        private:
            connection *conn_;
            
        public:
            explicit socket_connection_proxy(socket_client_session *parent, connection *conn);
            
            connection *get_connection() const {
                return conn_;
            }

            void dispatch(service::ipc_context *ctx) override;
            socket_subsession_type type() const override {
                return socket_subsession_type_connection;
            }
        };
        
        class socket_host_resolver: public socket_subsession {
            std::unique_ptr<host_resolver> resolver_;
            connection *conn_;

        protected:
            void get_host_name(service::ipc_context *ctx);
            void set_host_name(service::ipc_context *ctx);
            void close(service::ipc_context *ctx);

        public:
            explicit socket_host_resolver(socket_client_session *parent, std::unique_ptr<host_resolver> &resolver,
                connection *conn = nullptr);

            void dispatch(service::ipc_context *ctx) override;

            socket_subsession_type type() const override {
                return socket_subsession_type_host_resolver;
            }
        };

        class socket_socket: public socket_subsession {
            std::unique_ptr<socket> sock_;

        protected:
            void get_option(service::ipc_context *ctx);
            void close(service::ipc_context *ctx);

        public:
            explicit socket_socket(socket_client_session *parent, std::unique_ptr<socket> &sock);

            void dispatch(service::ipc_context *ctx) override;

            socket_subsession_type type() const override {
                return socket_subsession_type_socket;
            }
        };
    }

    enum socket_opcode {
        socket_pr_find = 0x02,
        socket_so_create = 0x06,
        socket_so_get_opt = 0x18,
        socket_so_close = 0x1D,
        socket_hr_open = 0x28,
        socket_hr_open_with_connection = 0x3E,
        socket_sr_get_by_number = 0x3F,
        socket_cn_get_long_des_setting = 0x51
    };

    enum socket_opcode_old {
        socket_old_num_pr = 0x00,
        socket_old_pr_info = 0x01,
        socket_old_pr_find = 0x02,
        socket_old_so_create = 0x06,
        socket_old_hr_open = 0x24,
        socket_old_hr_get_host_name = 0x28,
        socket_old_hr_set_host_name = 0x29,
        socket_old_hr_close = 0x2B
    };

    struct protocol_description {
        epoc::buf_static<char16_t, 32> name_;
        std::uint32_t addr_fam_;
        std::uint32_t sock_type_;
        std::uint32_t protocol_;
        epoc::version ver_;
        epoc::socket::byte_order bord_;
        std::uint32_t service_info_;
        std::uint32_t naming_services_;
        std::uint32_t service_sec_;
        std::uint32_t message_size_;
    };

    std::string get_socket_server_name_by_epocver(const epocver ver);
    
    class socket_server : public service::typical_server {
        std::map<std::uint64_t, std::unique_ptr<epoc::socket::protocol>> protocols_;
        std::vector<std::unique_ptr<epoc::socket::connect_agent>> agents_;

    public:
        explicit socket_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;

        epoc::socket::protocol *find_protocol(const std::uint32_t addr_family, const std::uint32_t protocol_id);
        epoc::socket::protocol *find_protocol_by_name(const std::u16string &name);
        epoc::socket::connect_agent *get_connect_agent(const std::u16string &name);

        bool add_protocol(std::unique_ptr<epoc::socket::protocol> &pr);
        bool add_agent(std::unique_ptr<epoc::socket::connect_agent> &ag);
    };

    using socket_subsession_instance = std::unique_ptr<epoc::socket::socket_subsession>;

    struct socket_client_session : public service::typical_session {
    private:
        friend class epoc::socket::socket_host_resolver;
        friend class epoc::socket::socket_socket;

        common::identity_container<socket_subsession_instance> subsessions_;

    public:
        explicit socket_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);
        bool is_oldarch();

        void fetch(service::ipc_context *ctx) override;

        void hr_create(service::ipc_context *ctx, const bool with_conn);
        void so_create(service::ipc_context *ctx);
        void pr_find(service::ipc_context *ctx);
        void sr_get_by_number(eka2l1::service::ipc_context *ctx);
        void cn_get_long_des_setting(eka2l1::service::ipc_context *ctx);
    };
}
