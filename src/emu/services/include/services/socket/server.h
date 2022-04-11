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
#include <services/socket/host.h>
#include <services/socket/protocol.h>

#include <common/container.h>
#include <kernel/server.h>
#include <services/framework.h>

#include <utils/des.h>

#include <map>
#include <memory>

namespace eka2l1 {
    namespace epoc::socket {
        class socket_connection_proxy : public socket_subsession {
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

        class socket_host_resolver : public socket_subsession {
            std::unique_ptr<host_resolver> resolver_;
            connection *conn_;

        protected:
            void get_host_name(service::ipc_context *ctx);
            void set_host_name(service::ipc_context *ctx);
            void get_by_name(service::ipc_context *ctx);
            void close(service::ipc_context *ctx);

        public:
            explicit socket_host_resolver(socket_client_session *parent, std::unique_ptr<host_resolver> &resolver,
                connection *conn = nullptr);

            void dispatch(service::ipc_context *ctx) override;

            socket_subsession_type type() const override {
                return socket_subsession_type_host_resolver;
            }
        };

        class socket_socket : public socket_subsession {
            std::unique_ptr<socket> sock_;

        protected:
            void get_option(service::ipc_context *ctx);
            void set_option(service::ipc_context *ctx);
            void bind(service::ipc_context *ctx);
            void write(service::ipc_context *ctx);
            void ioctl(service::ipc_context *ctx);
            void close(service::ipc_context *ctx);
            void connect(service::ipc_context *ctx);
            void send(service::ipc_context *ctx, const bool has_return_length, const bool has_addr);
            void recv(service::ipc_context *ctx, const bool has_return_length, const bool one_or_more, const bool has_addr);
            void cancel_send(service::ipc_context *ctx);
            void cancel_recv(service::ipc_context *ctx);

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
        socket_so_send = 0x08,
        socket_so_send_no_len = 0x09,
        socket_so_recv = 0x0A,
        socket_so_recv_no_len = 0x0B,
        socket_so_recv_one_or_more = 0x0C,
        socket_so_write = 0xE,
        socket_so_connect = 0x13,
        socket_so_bind = 0x14,
        socket_so_get_opt = 0x18,
        socket_so_ioctl = 0x19,
        socket_so_close = 0x1D,
        socket_so_cancel_recv = 0x20,
        socket_so_cancel_send = 0x21,
        socket_so_cancel_connect = 0x22,
        socket_hr_open = 0x28,
        socket_hr_get_by_name = 0x29,
        socket_hr_close = 0x2F,
        socket_sr_get_by_number = 0x32,
        socket_so_open_with_connection = 0x3D,
        socket_hr_open_with_connection = 0x3E,
        socket_cn_open_with_cn_type = 0x3F,
        socket_cn_get_long_des_setting = 0x51,
        socket_so_open_with_subconnection = 0x71,
    };

    enum socket_opcode_reform {
        socket_reform_so_create = 0x20,
        socket_reform_so_send = 0x21,
        socket_reform_so_send_no_len = 0x22,
        socket_reform_so_recv = 0x23,
        socket_reform_so_recv_no_len = 0x24,
        socket_reform_so_recv_one_or_more = 0x25,
        socket_reform_so_read = 0x27,
        socket_reform_so_write = 0x28,
        socket_reform_so_send_to = 0x29,
        socket_reform_so_send_to_no_len = 0x2A,
        socket_reform_so_recv_from = 0x2B,
        socket_reform_so_recv_from_no_len = 0x2C,
        socket_reform_so_connect = 0x2D,
        socket_reform_so_bind = 0x2E,
        socket_reform_so_set_opt = 0x31,
        socket_reform_so_get_opt = 0x32,
        socket_reform_so_ioctl = 0x33,
        socket_reform_hr_open = 0x37,
        socket_reform_hr_get_by_name = 0x38,
        socket_reform_hr_next = 0x39,
        socket_reform_hr_get_by_addr = 0x3A,
        socket_reform_hr_set_host_name = 0x3C,
        socket_reform_sr_get_by_number = 0x3F,
        socket_reform_so_open_with_conn = 0x46,
        socket_reform_hr_open_with_connection = 0x47,
        socket_reform_cn_open_with_cn_type = 0x48,
        socket_reform_cn_get_long_des_setting = 0x51,
        socket_reform_pr_find = 0x82,
        socket_reform_so_local_name = 0x86,
        socket_reform_so_cancel_recv = 0x8A,
        socket_reform_so_cancel_send = 0x8B,
        socket_reform_so_cancel_connect = 0x8C,
        socket_reform_so_close = 0x88,
        socket_reform_so_cancel_ioctl = 0x89,
        socket_reform_hr_cancel = 0x91,
        socket_reform_hr_close = 0x92,
        socket_reform_so_open_with_subconn = 0xBC,
        socket_reform_ss_request_optimal_dealer = 0xBE
    };

    enum socket_opcode_old {
        socket_old_num_pr = 0x00,
        socket_old_pr_info = 0x01,
        socket_old_pr_find = 0x02,
        socket_old_so_create = 0x06,
        socket_old_so_set_opt = 0x13,
        socket_old_so_get_opt = 0x14,
        socket_old_so_close = 0x19,
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

    struct socket_open_info {
        std::uint32_t addr_family_;
        std::uint32_t socket_type_;
        std::uint32_t protocol_;
        std::uint32_t handle_;
        std::int32_t reserved_;
    };

    std::string get_socket_server_name_by_epocver(const epocver ver);

    class socket_server : public service::typical_server {
        std::vector<std::unique_ptr<epoc::socket::protocol>> protocols_;
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
        void so_create_with_conn_or_subconn(service::ipc_context *ctx);
        void pr_find(service::ipc_context *ctx);
        void sr_get_by_number(eka2l1::service::ipc_context *ctx);
        void cn_open(eka2l1::service::ipc_context *ctx);
        void cn_get_long_des_setting(eka2l1::service::ipc_context *ctx);
        void ss_request_optimal_dealer(eka2l1::service::ipc_context *ctx);
    };
}
