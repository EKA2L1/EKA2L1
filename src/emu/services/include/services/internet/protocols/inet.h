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

#include <services/internet/protocols/common.h>
#include <services/socket/protocol.h>
#include <services/socket/socket.h>

#include <common/container.h>
#include <common/sync.h>

#include <utils/des.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

struct addrinfo;
struct sockaddr;

namespace eka2l1::epoc::internet {
    class midman;
    class inet_bridged_protocol;

    struct sinet_address: public socket::saddress {
        static constexpr std::uint32_t DATA_SIZE = 12;

        std::uint32_t *addr_long() {
            return reinterpret_cast<std::uint32_t*>(user_data_);
        }
    };

    struct sinet6_address: public socket::saddress {
        static constexpr std::uint32_t DATA_SIZE = 32;

        std::uint32_t *address_32x4() {
            return reinterpret_cast<std::uint32_t*>(user_data_);
        }

        std::uint32_t *flow() {
            return reinterpret_cast<std::uint32_t*>(user_data_) + 4;
        }

        std::uint32_t *scope() {
            return reinterpret_cast<std::uint32_t*>(user_data_) + 5;
        }
        
        const std::uint32_t *get_address_32x4() const {
            return reinterpret_cast<const std::uint32_t*>(user_data_);
        }

        std::uint32_t get_flow() const {
            return user_data_[4];
        }

        std::uint32_t get_scope() const {
            return user_data_[5];
        }
    };

    enum inet_interface_status : std::uint32_t {
        inet_interface_status_pending = 0,
        inet_interface_status_up = 1,
        inet_interface_status_busy = 2,
        inet_interface_status_down = 3
    };

    struct inet_interface_info {
        epoc::name tag_;
        epoc::name name_;
        inet_interface_status status_;
        std::int32_t mtu_;
        std::int32_t speed_metric_;

        std::uint32_t features_;
        std::uint32_t hardware_addr_len_;
        std::uint32_t hardware_addr_max_len_;
        socket::saddress hardware_addr_;
        std::uint32_t addr_len_;
        std::uint32_t addr_max_len_;
        sinet_address addr_;
        std::uint32_t netmask_addr_len_;
        std::uint32_t netmask_addr_max_len_;
        sinet_address netmask_addr_;
        std::uint32_t broadcast_addr_len_;
        std::uint32_t broadcast_addr_max_len_;
        sinet_address broadcast_addr_;
        std::uint32_t default_gateway_len_;
        std::uint32_t default_gateway_max_len_;
        sinet_address default_gateway_;
        std::uint32_t primary_name_server_len_;
        std::uint32_t primary_name_server_max_len_;
        sinet_address primary_name_server_;
        std::uint32_t secondary_name_server_len_;
        std::uint32_t secondary_name_server_max_len_;
        sinet_address secondary_name_server_; 
    };

    static_assert(sizeof(inet_interface_info) == 824);

    class inet_host_resolver : public socket::host_resolver {
    private:
        inet_bridged_protocol *papa_;
        std::uint32_t addr_family_;
        std::uint32_t protocol_id_;

        addrinfo *prev_info_;
        addrinfo *iterating_info_;

    public:
        explicit inet_host_resolver(inet_bridged_protocol *papa, const std::uint32_t addr_family, const std::uint32_t protocol_id);
        ~inet_host_resolver();

        std::u16string host_name() const override;
        bool host_name(const std::u16string &name) override;

        bool get_by_address(epoc::socket::saddress &addr, epoc::socket::name_entry &result) override;
        bool get_by_name(epoc::socket::name_entry &supply_and_result) override;
        bool next(epoc::socket::name_entry &result) override;
    };

    struct inet_socket : public socket::socket {
    private:
        inet_bridged_protocol *papa_;

        void *opaque_handle_;
        void *opaque_connect_;
        void *opaque_send_info_;
        void *opaque_write_info_;
        void *opaque_interface_info_;
        void *opaque_interface_info_current_;

        std::uint32_t protocol_;
        epoc::notify_info connect_done_info_;
        epoc::notify_info send_done_info_;
        epoc::notify_info recv_done_info_;

        std::uint32_t *bytes_written_;
        std::uint32_t *bytes_read_;
        std::uint8_t *read_dest_;

        std::size_t recv_size_;
        bool take_available_only_;

        std::vector<char> temp_buffer_;
        epoc::socket::saddress listen_addr_;
        epoc::socket::receive_done_callback receive_done_cb_;

        std::unique_ptr<common::ring_buffer<char, 0x80000>> stream_data_buffer_;
        common::event exit_event_;
        common::event open_event_;

        void close_down();
        void handle_connect_done_error_code(const int error_code);
        bool start_enumerate_network_interfaces();
        std::size_t retrieve_next_interface_info(std::uint8_t *buffer, const std::size_t avail_size);

    public:
        explicit inet_socket(inet_bridged_protocol *papa)
            : papa_(papa)
            , opaque_handle_(nullptr)
            , opaque_connect_(nullptr)
            , opaque_send_info_(nullptr)
            , opaque_write_info_(nullptr)
            , opaque_interface_info_(nullptr)
            , opaque_interface_info_current_(nullptr)
            , protocol_(0)
            , bytes_written_(nullptr)
            , bytes_read_(nullptr)
            , read_dest_(nullptr)
            , recv_size_(0)
            , take_available_only_(false)
            , stream_data_buffer_(nullptr)
            , receive_done_cb_(nullptr) {
        }

        ~inet_socket() override;

        bool open(const std::uint32_t family_id, const std::uint32_t protocol_id, const epoc::socket::socket_type sock_type);
        void connect(const epoc::socket::saddress &addr, epoc::notify_info &info) override;
        void bind(const epoc::socket::saddress &addr, epoc::notify_info &info) override;
        void send(const std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *sent_size, const epoc::socket::saddress *addr, std::uint32_t flags, epoc::notify_info &complete_info) override;
        void receive(std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *sent_size, const epoc::socket::saddress *addr,
            std::uint32_t flags, epoc::notify_info &complete_info, epoc::socket::receive_done_callback done_callback) override;
        std::int32_t local_name(epoc::socket::saddress &result, std::uint32_t &result_len) override;
        std::int32_t remote_name(epoc::socket::saddress &result, std::uint32_t &result_len) override;

        std::size_t get_option(const std::uint32_t option_id, const std::uint32_t option_family,
            std::uint8_t *buffer, const std::size_t avail_size) override;
        bool set_option(const std::uint32_t option_id, const std::uint32_t option_family,
            std::uint8_t *buffer, const std::size_t avail_size) override;

        void cancel_receive() override;
        void cancel_send() override;
        void cancel_connect() override;

        void complete_connect_done_info(const int err);
        void complete_send_done_info(const int err);
        void prepare_buffer_for_recv(const std::size_t suggested_size, void *buf_ptr);
        void handle_udp_delivery(const std::int64_t bytes_read, const void *buf_ptr, const void *addr);
        void handle_tcp_delivery(const std::int64_t bytes_read, const void *buf_ptr);

        void set_exit_event();
        void *get_opaque_handle() {
            return opaque_handle_;
        }
    };

    class inet_bridged_protocol : public socket::protocol {
    private:
        std::unique_ptr<std::thread> loop_thread_;

    public:
        explicit inet_bridged_protocol(const bool oldarch);
        ~inet_bridged_protocol() override;

        void initialize_looper();

        virtual std::u16string name() const override {
            return u"INet";
        }

        std::vector<std::uint32_t> family_ids() const override {
            return { INET_ADDRESS_FAMILY, INET6_ADDRESS_FAMILY };
        }

        virtual std::vector<std::uint32_t> supported_ids() const override {
            return { INET_UDP_PROTOCOL_ID, INET_TCP_PROTOCOL_ID };
        }

        virtual epoc::version ver() const override {
            epoc::version v;
            v.major = 0;
            v.minor = 0;
            v.build = 1;

            return v;
        }

        virtual epoc::socket::byte_order get_byte_order() const override {
            return epoc::socket::byte_order_little_endian;
        }

        std::unique_ptr<epoc::socket::socket> make_socket(const std::uint32_t family_id, const std::uint32_t protocol_id, const socket::socket_type sock_type) override;

        virtual std::unique_ptr<epoc::socket::host_resolver> make_host_resolver(const std::uint32_t family_id, const std::uint32_t protocol_id) override {
            return std::make_unique<inet_host_resolver>(this, family_id, protocol_id);
        }
    };

    void host_sockaddr_to_guest_saddress(sockaddr *addr, epoc::socket::saddress &dest_addr, std::uint32_t *data_len = nullptr);
}
