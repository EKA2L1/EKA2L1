/*
 * Copyright (c) 2022 EKA2L1 Team
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

#include <services/bluetooth/protocols/btmidman_inet.h>
#include <common/log.h>
#include <common/algorithm.h>

namespace eka2l1::epoc::bt {
    midman_inet::midman_inet(const int port)
        : midman()
        , allocated_ports_(MAX_PORT)
        , virt_bt_info_server_(nullptr)
        , port_(port) {
        /** ================================= DEBUG ====================================== */
        // Leave it here for the comeback later
        /*
        hosters_[0].family_ = hosters_[1].family_ = hosters_[2].family_ = hosters_[3].family_ = 0;
        hosters_[0].family_ = epoc::internet::INET6_ADDRESS_FAMILY;

        uint16_t *arr_ip = reinterpret_cast<uint16_t*>(hosters_[0].address_32x4());
        std::memset(arr_ip, 0, 16);
        arr_ip[7] = 256;

        hosters_[0].port_ = 46689;
        */
        /** ================================= END DEBUG ====================================== */

        virt_bt_info_server_ = new uv_udp_t;
        if (uv_udp_init(uv_default_loop(), virt_bt_info_server_) < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to initialize INET Bluetooth info server!");
        }

        virt_bt_info_server_->data = this;

        sockaddr_in6 addr_bind;
        std::memset(&addr_bind, 0, sizeof(sockaddr_in6));
        addr_bind.sin6_family = AF_INET6;
        addr_bind.sin6_port = htons(static_cast<std::uint16_t>(port));
        addr_bind.sin6_scope_id = 0;

        uv_udp_bind(virt_bt_info_server_, reinterpret_cast<const sockaddr*>(&addr_bind), UV_UDP_IPV6ONLY);
        uv_udp_recv_start(virt_bt_info_server_, [](uv_handle_t* handle, std::size_t suggested_size, uv_buf_t* buf) {
            reinterpret_cast<midman_inet*>(handle->data)->prepare_server_recv_buffer(buf, suggested_size);
        }, [](uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags) {
            if (nread <= 0) {
                return;
            }

            reinterpret_cast<midman_inet*>(handle->data)->handle_server_request(addr, buf, nread);
        });
    }

    midman_inet::~midman_inet() {
        uv_udp_recv_stop(virt_bt_info_server_);
        uv_close(reinterpret_cast<uv_handle_t*>(virt_bt_info_server_), [](uv_handle_t *hh) {
            delete hh;
        });
    }

    void midman_inet::prepare_server_recv_buffer(uv_buf_t *buf, const std::size_t suggested_size) {
        server_recv_buf_.resize(suggested_size);
        
        buf->base = server_recv_buf_.data();
        buf->len = static_cast<std::uint32_t>(suggested_size);
    }

    void midman_inet::handle_server_request(const sockaddr *requester, const uv_buf_t *buf, ssize_t nread) {
        uv_udp_send_t *send_req = new uv_udp_send_t;
        uv_buf_t local_buf;
        std::uint32_t temp_uint;
        
        if (buf->base[0] == 'n') {
            std::string device_name_utf8 = common::ucs2_to_utf8(device_name());
            local_buf = uv_buf_init(device_name_utf8.data(), static_cast<std::uint32_t>(device_name_utf8.length()));
        } else if (buf->base[0] == 'p') {
            std::uint16_t virtual_port = buf->base[1] | (static_cast<std::uint16_t>(buf->base[2]) << 8);
            temp_uint = lookup_host_port(virtual_port);

            local_buf = uv_buf_init(reinterpret_cast<char*>(&temp_uint), 4);
        }

        uv_udp_send(send_req, virt_bt_info_server_, &local_buf, 1, requester, [](uv_udp_send_t *send_info, int status) {
            delete send_info;
        });
    }

    bool midman_inet::get_friend_address(const std::uint32_t index, internet::sinet6_address &addr) {
        const std::lock_guard<std::mutex> guard(hosters_lock_);
        if ((index >= hosters_.size()) && (hosters_[index].family_ == 0)) {
            return false;
        }

        addr = hosters_[index];
        return true;
    }

    std::uint32_t midman_inet::lookup_host_port(const std::uint16_t virtual_port) {
        auto result = port_map_.find(virtual_port);
        if (result == port_map_.end()) {
            return 0;
        }

        return result->second;
    }

    void midman_inet::add_host_port(const std::uint16_t virtual_port, const std::uint32_t host_port) {
        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} is out of allowed range!", virtual_port);
            return;
        }

        if (port_map_.find(virtual_port) != port_map_.end()) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} already mapped to host!", virtual_port);
            return;
        }

        port_map_[virtual_port] = host_port;
        allocated_ports_.force_fill(virtual_port - 1, 1);
    }
    
    std::uint16_t midman_inet::get_free_port() {
        int size = 1;
        const int offset = allocated_ports_.allocate_from(0, size, false);
        if (offset < 0) {
            return 0;
        }

        return static_cast<std::uint16_t>(offset + 1);
    }

    void midman_inet::close_port(const std::uint16_t virtual_port) {
        allocated_ports_.deallocate(virtual_port - 1, 1);
    }
}