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
#include <services/bluetooth/protocols/common_inet.h>
#include <services/internet/protocols/common.h>
#include <services/internet/protocols/inet.h>

#include <common/random.h>
#include <common/log.h>
#include <common/algorithm.h>
#include <common/thread.h>
#include <config/config.h>
#include <common/upnp.h>

namespace eka2l1::epoc::bt {
    bool midman_inet::should_upnp_apply_to_port() {
        return enable_upnp_ && (discovery_mode_ == DISCOVERY_MODE_PROXY_SERVER) && (server_addr_.family_ == internet::INET_ADDRESS_FAMILY);
    }

    midman_inet::midman_inet(const config::state &conf)
        : midman()
        , allocated_ports_(MAX_PORT)
        , port_offset_(conf.btnet_port_offset)
        , enable_upnp_(conf.enable_upnp)
        , friend_info_cached_(false)
        , lan_discovery_call_listener_socket_(nullptr)
        , bluetooth_queries_server_socket_(nullptr)
        , matching_server_socket_(nullptr)
        , hearing_timeout_timer_(nullptr)
        , port_(conf.internet_bluetooth_port)
        , retried_lan_discovery_times_(0)
        , device_addr_asker_(this)
        , current_active_observer_(nullptr)
        , password_(conf.btnet_password)
        , discovery_mode_(static_cast<discovery_mode>(conf.btnet_discovery_mode))
        , asker_counter_(0) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if (discovery_mode_ != DISCOVERY_MODE_DIRECT_IP) {
            port_ = HARBOUR_PORT;
        }

        std::fill(port_refs_.begin(), port_refs_.end(), 0);

        std::vector<std::uint64_t> errs;
        update_friend_list(conf.friend_addresses, errs);

        for (std::size_t i = 0; i < errs.size(); i++) {
            std::uint32_t index = static_cast<std::uint32_t>(errs[i] & 0xFFFFFFFF);
            if (errs[i] & FRIEND_UPDATE_ERROR_INVALID_PORT_NUMBER) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Bluetooth netplay friend address number {} has invalid port number ({})", index + 1, conf.friend_addresses[index].port_);
            } else if (errs[i] & FRIEND_UPDATE_ERROR_INVALID_ADDR) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Bluetooth netplay friend address number {} has invalid address ({})", index + 1, conf.friend_addresses[index].addr_);
            }
        }

        for (std::uint32_t i = 0; i < 6; i++) {
            random_device_addr_.addr_[i] = static_cast<std::uint8_t>(random_range(0, 0xFF));
        }

        auto looper = libuv::default_looper;
        std::string bt_server_url = conf.bt_central_server_url;

        if (!looper->started()) {
            looper->set_loop_thread_prepare_callback([]() { common::set_thread_priority(common::thread_priority_very_high); });
            looper->start();
        }

        looper->one_shot([this, bt_server_url]() {
            auto loop = uvw::loop::get_default();

            bluetooth_queries_server_socket_ = loop->resource<uvw::udp_handle>();
            hearing_timeout_timer_ = loop->resource<uvw::timer_handle>();

            if (discovery_mode_ == DISCOVERY_MODE_LAN) {
                setup_lan_discovery();
            } else if (discovery_mode_ == DISCOVERY_MODE_PROXY_SERVER) {
                setup_proxy_server_discovery(bt_server_url);
            }

            sockaddr_in6 addr_bind;
            std::memset(&addr_bind, 0, sizeof(sockaddr_in6));
            addr_bind.sin6_family = (discovery_mode_ == DISCOVERY_MODE_LAN) ? AF_INET : AF_INET6;
            addr_bind.sin6_port = htons(static_cast<std::uint16_t>(port_));

            bluetooth_queries_server_socket_->bind(*reinterpret_cast<sockaddr*>(&addr_bind));

            if (should_upnp_apply_to_port()) {
                UPnP::TryPortmapping(static_cast<std::uint16_t>(port_), true);
            }

            bluetooth_queries_server_socket_->on<uvw::udp_data_event>([this](const uvw::udp_data_event &event, uvw::udp_handle &handle) {
                std::optional<sockaddr_in6> sender_ced = libuv::from_ip_string(event.sender.ip.data(), event.sender.port);
                if (!sender_ced.has_value()) {
                    LOG_ERROR(SERVICE_BLUETOOTH, "Invalid sender address passed to callback!");
                    return;
                }
                handle_queries_request(reinterpret_cast<sockaddr*>(&sender_ced.value()), event.data.get(), event.length);
            });

            bluetooth_queries_server_socket_->on<uvw::error_event>([this](const uvw::error_event &event, uvw::udp_handle &handle) {
                handle_queries_request(nullptr, nullptr, event.code());
            });

            bluetooth_queries_server_socket_->recv();
        });
    }

    midman_inet::~midman_inet() {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if (should_upnp_apply_to_port()) {
            UPnP::StopPortmapping(static_cast<std::uint16_t>(port_), true);

            for (std::size_t i = 0; i < port_refs_.size(); i++) {
                if (port_refs_[i] != 0) {
                    UPnP::StopPortmapping(static_cast<std::uint16_t>(port_offset_ + i), false);
                }
            }
        }

        send_logout(true);

        auto lan_discovery_call_listener_socket_copy = lan_discovery_call_listener_socket_;
        auto bluetooth_queries_server_socket_copy = bluetooth_queries_server_socket_;
        auto hearing_timeout_timer_copy = hearing_timeout_timer_;

        libuv::default_looper->one_shot([lan_discovery_call_listener_socket_copy, bluetooth_queries_server_socket_copy, hearing_timeout_timer_copy]() {
        });
    }

    void midman_inet::on_timeout_friend_search() {
        const std::lock_guard<std::mutex> guard(friends_lock_);

        if ((++retried_lan_discovery_times_ <= RETRY_LAN_DISCOVERY_TIME_MAX) && (discovery_mode_ == DISCOVERY_MODE_LAN)) {
            send_call_for_strangers();
            return;
        }

        if (current_active_observer_) {
            hearing_timeout_timer_->stop();

            current_active_observer_->on_no_more_strangers();
            current_active_observer_ = nullptr;

            if (!pending_observers_.empty()) {
                inet_stranger_call_observer *obs = pending_observers_.front();
                pending_observers_.erase(pending_observers_.begin());

                current_active_observer_ = obs;
                send_call_for_strangers();
            }
        }
    }

    void midman_inet::reset_friend_timeout_timer() {
        if (!reset_timeout_timer_task_) {
            reset_timeout_timer_task_ = libuv::create_task([this]() {
                const std::uint32_t duration = (discovery_mode_ == DISCOVERY_MODE_LAN) ? TIMEOUT_HEARING_STRANGER_LAN_MS : TIMEOUT_HEARING_STRANGER_MS;
                const auto duration_chrono = std::chrono::milliseconds(duration);

                hearing_timeout_timer_->stop();

                hearing_timeout_timer_->on<uvw::timer_event>([this](const uvw::timer_event &event, uvw::timer_handle &handle) {
                    on_timeout_friend_search();
                });

                hearing_timeout_timer_->start(duration_chrono, duration_chrono);
            });
        }

        libuv::default_looper->post_task(reset_timeout_timer_task_);
    }

    void midman_inet::handle_queries_request(const sockaddr *sender, const char *buf, std::int64_t nread) {
        const std::uint32_t asker_id = *reinterpret_cast<const std::uint32_t*>(buf);
        query_opcode opcode = static_cast<query_opcode>(buf[4]);
        char opcode_result_signature = QUERY_OPCODE_RESULT_START + opcode;

        switch (opcode) {
        case QUERY_OPCODE_GET_NAME: {
            std::string name_utf8 = common::ucs2_to_utf8(device_name());
            name_utf8.insert(name_utf8.begin(), static_cast<char>(name_utf8.length()));
            name_utf8.insert(name_utf8.begin(), opcode_result_signature);
            name_utf8.insert(name_utf8.begin(), reinterpret_cast<const char*>(&asker_id), reinterpret_cast<const char*>(&asker_id + 1));

            bluetooth_queries_server_socket_->send(*sender, name_utf8.data(), static_cast<std::uint32_t>(name_utf8.size()));
            break;
        }

        case QUERY_OPCODE_IS_REAL_PORT_MAPPED_TO_VIRTUAL_PORT: {
            std::uint32_t requested_port = static_cast<std::uint8_t>(buf[5]) |
                (static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf[6])) << 8) |
                (static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf[7])) << 16) |
                (static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf[8])) << 24);

            std::vector<char> check_result;
            check_result.insert(check_result.end(), reinterpret_cast<const char*>(&asker_id), reinterpret_cast<const char*>(&asker_id + 1));
            check_result.push_back(opcode_result_signature);

            char final_result  = '0';

            if ((requested_port >= port_offset_) && (requested_port < port_offset_ + MAX_PORT)) {
                final_result = '1';
            }

            check_result.push_back(final_result);

            bluetooth_queries_server_socket_->send(*sender, check_result.data(), static_cast<std::uint32_t>(check_result.size()));
            break;
        }

        case QUERY_OPCODE_GET_REAL_PORT_FROM_VIRTUAL_PORT: {
            std::uint16_t virtual_port = buf[5] | (static_cast<std::uint16_t>(buf[6]) << 8);
            std::uint32_t temp_uint = get_host_port(virtual_port);

            std::vector<char> buf_result;
            buf_result.insert(buf_result.begin(), reinterpret_cast<const char*>(&asker_id), reinterpret_cast<const char*>(&asker_id + 1));
            buf_result.push_back(opcode_result_signature);
            buf_result.insert(buf_result.end(), reinterpret_cast<char *>(&temp_uint), reinterpret_cast<char *>(&temp_uint + 1));

            bluetooth_queries_server_socket_->send(*sender, buf_result.data(), static_cast<std::uint32_t>(buf_result.size()));
            break;
        }

        case QUERY_OPCODE_GET_VIRTUAL_BLUETOOTH_ADDRESS: {
            std::vector<char> buf_result;
            buf_result.insert(buf_result.begin(), reinterpret_cast<const char*>(&asker_id), reinterpret_cast<const char*>(&asker_id + 1));
            buf_result.push_back(opcode_result_signature);
            buf_result.insert(buf_result.end(), reinterpret_cast<char *>(&random_device_addr_), reinterpret_cast<char *>(&random_device_addr_ + 1));

            bluetooth_queries_server_socket_->send(*sender, buf_result.data(), static_cast<std::uint32_t>(buf_result.size()));
            break;
        }

        default:
            LOG_ERROR(SERVICE_BLUETOOTH, "Unhandeled query opcode: {}", static_cast<int>(opcode));
            break;
        }
    }

    void midman_inet::add_friend(epoc::bt::friend_info &info) {
        const std::lock_guard<std::mutex> guard(friends_lock_);
        if (info.real_addr_.family_ == epoc::internet::INET_ADDRESS_FAMILY) {
            // Convert to ipv6
            epoc::internet::sinet6_address converted_addr;
            std::memset(&converted_addr, 0, sizeof(epoc::socket::saddress));

            converted_addr.family_ = epoc::internet::INET6_ADDRESS_FAMILY;
            converted_addr.port_ = info.real_addr_.port_;

            std::uint32_t *addr_value_ptr = converted_addr.address_32x4();

            std::memcpy(addr_value_ptr + 3, info.real_addr_.user_data_, 4);
            addr_value_ptr[2] = 0xFFFF0000;

            info.real_addr_ = converted_addr;
        }

        for (std::size_t i = 0; i < friends_.size(); i++) {
            if (std::memcmp(&friends_[i].real_addr_, &info.real_addr_, sizeof(epoc::socket::saddress)) == 0) {
                if (friends_[i].refreshed_) {
                    return;
                }

                current_active_observer_->on_stranger_call(info.real_addr_, static_cast<std::uint32_t>(i));
                friends_[i].refreshed_ = true;
                return;
            }
        }
    
        friends_.push_back(info);
        current_active_observer_->on_stranger_call(info.real_addr_, static_cast<std::uint32_t>(friends_.size() - 1));
    }

    void midman_inet::read_and_add_friend(const char *buf, char &buf_pointer) {
        epoc::bt::friend_info info;
        info.dvc_addr_.padding_ = 0;

        char is_ipv4 = buf[buf_pointer++];
        if (is_ipv4) {
            info.real_addr_.family_ = epoc::internet::INET_ADDRESS_FAMILY;

            *static_cast<epoc::internet::sinet_address &>(info.real_addr_).addr_long() = *reinterpret_cast<const std::uint32_t *>(buf);
            buf_pointer += sizeof(std::uint32_t);
        } else {info.real_addr_.family_ = epoc::internet::INET6_ADDRESS_FAMILY;

            std::memcpy(static_cast<epoc::internet::sinet6_address &>(info.real_addr_).address_32x4(), buf, sizeof(std::uint32_t) * 4);
            buf_pointer += sizeof(std::uint32_t) * 4;
        }

        info.real_addr_.port_ = HARBOUR_PORT;
        add_friend(info);
    }

    bool midman_inet::get_friend_address(const device_address &friend_addr, epoc::socket::saddress &addr) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return false;
        }

        const std::lock_guard<std::mutex> guard(friends_lock_);

lookup:
        for (std::size_t i = 0; i < friends_.size(); i++) {
            if (std::memcmp(friend_addr.addr_, friends_[i].dvc_addr_.addr_, 6) == 0) {
                addr = friends_[i].real_addr_;
                return true;
            }
        }

        if (!friend_info_cached_) {
            // Try to refresh the local cache. It's not really ideal, but anyway resolver always redo
            // a full rescan...
            refresh_friend_infos();
            goto lookup;
        }

        return false;
    }
    

    void midman_inet::get_friend_address_async(const device_address &friend_virt_addr, std::function<void(epoc::socket::saddress*)> callback) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            callback(nullptr);
            return;
        }

        const std::lock_guard<std::mutex> guard(friends_lock_);

        auto check_for_friend_and_run_cb = [this, callback, friend_virt_addr]() {
            for (std::size_t i = 0; i < friends_.size(); i++) {
                if (std::memcmp(friend_virt_addr.addr_, friends_[i].dvc_addr_.addr_, 6) == 0) {
                    callback(&friends_[i].real_addr_);
                    return true;
                }
            }

            return false;
        };

        if (check_for_friend_and_run_cb()) {
            return;
        }

        if (!friend_info_cached_) {
            // Try to refresh the local cache. It's not really ideal, but anyway resolver always redo
            // a full rescan...
            refresh_friend_infos_async([this, check_for_friend_and_run_cb]() {
                check_for_friend_and_run_cb();
            });
        }
    }

    bool midman_inet::get_friend_address(const std::uint32_t index, epoc::socket::saddress &addr) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return false;
        }
        const std::lock_guard<std::mutex> guard(friends_lock_);
        if ((index >= friends_.size()) && (friends_[index].real_addr_.family_ == 0)) {
            return false;
        }

        addr = friends_[index].real_addr_;
        return true;
    }

    void midman_inet::add_device_address_mapping(const std::uint32_t index, const device_address &addr) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        const std::lock_guard<std::mutex> guard(friends_lock_);

        if ((friends_[index].dvc_addr_.padding_ == 1) || (std::memcmp(friends_[index].dvc_addr_.addr_, addr.addr_, 6) != 0)) {
            friends_[index].dvc_addr_ = addr;
            friends_[index].dvc_addr_.padding_ = 0;
        }
    }

    void midman_inet::refresh_friend_infos() {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if (friend_info_cached_) {
            return;
        }

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(friends_.size()); i++) {
            if (friends_[i].real_addr_.family_ != 0) {
                if (std::optional<device_address> result = device_addr_asker_.get_device_address(friends_[i].real_addr_)) {
                    friends_[i].dvc_addr_ = result.value();
                    friend_device_address_mapping_.emplace(friends_[i].dvc_addr_, i);
                } else {
                    LOG_ERROR(SERVICE_BLUETOOTH, "Unable to get friend number {} virtual bluetooth MAC address", i);
                }
            }
        }

        friend_info_cached_ = true;
    }

    void midman_inet::refresh_friend_info_async_impl(std::uint32_t start_pos, std::function<void()> callback) {
        while ((start_pos < friends_.size()) && (friends_[start_pos].real_addr_.family_ == 0)) {
            start_pos++;
        }

        if (start_pos >= friends_.size()) {
            friend_info_cached_ = true;
            callback();

            return;
        }

        device_addr_asker_.get_device_address_async(friends_[start_pos].real_addr_, [this, start_pos, callback](device_address *result) {
            if (!result) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Unable to get friend number {} virtual bluetooth MAC address", start_pos);
            } else {
                friends_[start_pos].dvc_addr_ = *result;
                friend_device_address_mapping_.emplace(friends_[start_pos].dvc_addr_, start_pos);

                refresh_friend_info_async_impl(start_pos + 1, callback);
            }
        });
    }

    void midman_inet::refresh_friend_infos_async(std::function<void()> callback) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if (friend_info_cached_ || (friends_.size() == 0)) {
            return;
        }

        refresh_friend_info_async_impl(0, callback);
    }

    std::uint32_t midman_inet::get_host_port(const std::uint16_t virtual_port) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return 0;
        }

        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            return 0;
        }
        
        if (allocated_ports_.is_allocated(virtual_port - 1)) {
            return port_offset_ + virtual_port - 1;
        }

        return port_offset_ + virtual_port - 1;
    }

    void midman_inet::ref_and_public_port(const std::uint16_t virtual_port) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} is out of allowed range!", virtual_port);
            return;
        }

        if (should_upnp_apply_to_port()) {
            UPnP::TryPortmapping(virtual_port - 1 + port_offset_, false);
        }

        allocated_ports_.force_fill(virtual_port - 1, 1);
        port_refs_[virtual_port - 1] = 1;
    }
    
    std::uint16_t midman_inet::get_free_port() {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return 0;
        }

        // Reserve first 20 ports for system
        int size = 1;
        const int offset = allocated_ports_.allocate_from(20, size, false);
        if (offset < 0) {
            return 0;
        }

        return static_cast<std::uint16_t>(offset + 1);
    }

    void midman_inet::ref_port(const std::uint16_t virtual_port) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }
        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} is out of allowed range!", virtual_port);
            return;
        }
        
        port_refs_[virtual_port - 1]++;
    }

    void midman_inet::close_port(const std::uint16_t virtual_port) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} is out of allowed range!", virtual_port);
            return;
        }

        if (allocated_ports_.is_allocated(virtual_port - 1)) {
            std::uint32_t ref_count = --port_refs_[virtual_port - 1];
            if (ref_count == 0) {
                if (should_upnp_apply_to_port()) {
                    UPnP::StopPortmapping(virtual_port, false);
                }
                allocated_ports_.deallocate(virtual_port - 1, 1);
            }
        }
    }

    void midman_inet::update_friend_list(const std::vector<config::friend_address> &addrs, std::vector<std::uint64_t> &invalid_address_indicies) {
        const std::lock_guard<std::mutex> guard(friends_lock_);

        if (discovery_mode_ != DISCOVERY_MODE_DIRECT_IP) {
            return;
        }

        sockaddr_in6 in_temp;
        int res = 0;

        epoc::bt::friend_info info;
        info.dvc_addr_.padding_ = 1;

        for (std::size_t i = 0; i < common::min<std::size_t>(addrs.size(), MAX_INET_DEVICE_AROUND); i++) {
            // Max TCP port number
            if (addrs[i].port_ > 65535) {
                invalid_address_indicies.push_back(FRIEND_UPDATE_ERROR_INVALID_PORT_NUMBER | static_cast<std::uint32_t>(i));
            } else {
                std::string ip_str = addrs[i].addr_;
                if (addrs[i].addr_.find(':') == std::string::npos) {
                    ip_str = std::string("::ffff:") + ip_str;
                }

                res = uv_ip6_addr(ip_str.data(), addrs[i].port_, &in_temp);

                if (res != 0) {
                    invalid_address_indicies.push_back(FRIEND_UPDATE_ERROR_INVALID_ADDR | static_cast<std::uint32_t>(i));
                } else {
                    std::memset(&info.real_addr_, 0, sizeof(epoc::socket::saddress));
                    internet::host_sockaddr_to_guest_saddress(reinterpret_cast<sockaddr*>(&in_temp), info.real_addr_);
                
                    friends_.push_back(info);
                }
            }
        }
    }

    std::vector<std::uint32_t> midman_inet::get_friend_index_with_address(epoc::socket::saddress &addr) {
        bool is_ipv4 = (addr.family_ == epoc::internet::INET_ADDRESS_FAMILY);
        std::vector<std::uint32_t> indicies;

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(friends_.size()); i++) {
            if (friends_[i].real_addr_.family_ == 0) {
                continue;
            }

            bool is_current_ipv4 = (friends_[i].real_addr_.family_ == epoc::internet::INET_ADDRESS_FAMILY);
            if (is_current_ipv4 != is_ipv4) {
                continue;
            }

            if (is_ipv4 && (*static_cast<epoc::internet::sinet_address&>(friends_[i].real_addr_).addr_long() == 
                *(static_cast<epoc::internet::sinet_address&>(addr).addr_long()))) {
                indicies.push_back(i);
            } else if (!is_ipv4 && (std::memcmp(static_cast<epoc::internet::sinet6_address&>(friends_[i].real_addr_).address_32x4(),
                static_cast<epoc::internet::sinet6_address&>(addr).address_32x4(), sizeof(std::uint32_t) * 4) == 0)) {
                indicies.push_back(i);
            }
        }

        return indicies;
    }

    bool midman_inet::get_friend_device_address(const std::uint32_t index, device_address &result) {
        if (index >= friends_.size()) {
            return false;
        }

        if (!friend_info_cached_) {
            // Try to refresh the local cache. It's not really ideal, but anyway resolver always redo
            // a full rescan...
            refresh_friend_infos();
        }

        if (friends_[index].real_addr_.family_ == 0) {
            return false;
        }

        result = friends_[index].dvc_addr_;
        return true;
    }

    void midman_inet::add_or_update_friend(const epoc::socket::saddress &addr) {
        const std::lock_guard<std::mutex> guard(friends_lock_);

        for (std::size_t i = 0; i < friends_.size(); i++) {
            if (memcmp(&(friends_[i].real_addr_), &addr, sizeof(epoc::socket::saddress)) == 0) {
                friends_[i].dvc_addr_.padding_ = 1;
                return;
            }
        }

        friend_info info;
        info.real_addr_ = addr;
        info.dvc_addr_.padding_ = 1;

        friends_.push_back(info);
    }

    void midman_inet::send_call_for_strangers() {
        if (!send_strangers_call_task_) {
            send_strangers_call_task_ = libuv::create_task([this]() {
                sockaddr *server_addr_sock_ptr = nullptr;
                GUEST_TO_BSD_ADDR(server_addr_, server_addr_sock_ptr);

                char request_friends = QUERY_OPCODE_GET_PLAYERS;

                if (discovery_mode_ == DISCOVERY_MODE_LAN) {
                    sockaddr_in6 server_addr_modded;

                    // A bit of overflow would be ok, I guess))
                    std::memcpy(&server_addr_modded, server_addr_sock_ptr, sizeof(sockaddr_in6));
                    server_addr_modded.sin6_port = htons(LAN_DISCOVERY_PORT);

                    std::vector<char> broadcast_buf;
                    broadcast_buf.push_back(request_friends);
                    broadcast_buf.push_back(static_cast<char>(password_.length()));
                    broadcast_buf.insert(broadcast_buf.end(), password_.begin(), password_.end());

                    lan_discovery_call_listener_socket_->on<uvw::error_event>([](const uvw::error_event &event, uvw::udp_handle &handle) {
                        if (event.code() < 0) {
                            LOG_ERROR(SERVICE_BLUETOOTH, "Fail to send broadcast message to find nearby playable devices! Libuv error code={}", event.code());
                        }
                    });

                    lan_discovery_call_listener_socket_->on<uvw::send_event>([](const uvw::send_event &event, uvw::udp_handle &handle) {
                        LOG_TRACE(SERVICE_BLUETOOTH, "Sent lan discovery call!");
                    });

                    lan_discovery_call_listener_socket_->broadcast(true);
                    lan_discovery_call_listener_socket_->send(*reinterpret_cast<sockaddr*>(&server_addr_modded), broadcast_buf.data(), static_cast<std::uint32_t>(broadcast_buf.size()));
                } else {
                    matching_server_socket_->write(&request_friends, 1);
                }

                if ((discovery_mode_ != DISCOVERY_MODE_LAN) || (retried_lan_discovery_times_ == 0)) {
                    reset_friend_timeout_timer();
                }
            });
        }

        libuv::default_looper->post_task(send_strangers_call_task_);
    }

    void midman_inet::begin_hearing_stranger_call(inet_stranger_call_observer *observer) {
        const std::lock_guard<std::mutex> guard(friends_lock_);

        for (std::size_t i = 0; i < friends_.size(); i++) {
            friends_[i].refreshed_ = false;
        }

        retried_lan_discovery_times_ = 0;

        if (current_active_observer_ || !pending_observers_.empty()) {
            pending_observers_.push_back(observer);
        } else {
            current_active_observer_ = observer;
            send_call_for_strangers();
        }
    }

    void midman_inet::unregister_stranger_call_observer(inet_stranger_call_observer *observer) {
        const std::lock_guard<std::mutex> guard(friends_lock_);
        if (observer == current_active_observer_) {
            current_active_observer_ = nullptr;
            return;
        }

        auto ite = std::find(pending_observers_.begin(), pending_observers_.end(), observer);
        if (ite == pending_observers_.end()) {
            return;
        }

        pending_observers_.erase(ite);
    }
}
