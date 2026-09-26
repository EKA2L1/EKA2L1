/*
 * Copyright (c) 2023 EKA2L1 Team
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
#include <services/bluetooth/protocols/mdns.h>

#include <algorithm>
#include <cstring>

namespace eka2l1::epoc::bt {
    void midman_inet::sync_lan_friends() {
        if (!mdns_) return;
        const auto peers = mdns_->peers();
        std::vector<friend_info> discovered;
        for (const auto &peer : peers) {
            friend_info info{};
            epoc::internet::sinet6_address endpoint{};
            endpoint.family_ = epoc::internet::INET6_ADDRESS_FAMILY;
            endpoint.port_ = peer.endpoint.port_;
            endpoint.address_32x4()[2] = 0xFFFF0000;
            endpoint.address_32x4()[3] = htonl(*reinterpret_cast<const std::uint32_t *>(peer.endpoint.user_data_));
            info.real_addr_ = endpoint;
            info.dvc_addr_ = peer.address;
            discovered.push_back(info);
        }

        const std::lock_guard<std::mutex> guard(friends_lock_);
        for (auto &friend_entry : friends_) {
            const auto found = std::find_if(discovered.begin(), discovered.end(), [&](const friend_info &peer) {
                return std::memcmp(&peer.real_addr_, &friend_entry.real_addr_, sizeof(epoc::socket::saddress)) == 0
                    && std::memcmp(peer.dvc_addr_.addr_, friend_entry.dvc_addr_.addr_, 6) == 0;
            });
            if (found == discovered.end()) {
                friend_device_address_mapping_.erase(friend_entry.dvc_addr_);
                friend_entry.real_addr_.family_ = 0;
            }
        }
        for (const auto &peer : discovered) {
            auto found = std::find_if(friends_.begin(), friends_.end(), [&](const friend_info &entry) {
                return std::memcmp(&peer.real_addr_, &entry.real_addr_, sizeof(epoc::socket::saddress)) == 0;
            });
            if (found == friends_.end()) {
                found = std::find_if(friends_.begin(), friends_.end(), [](const friend_info &entry) {
                    return entry.real_addr_.family_ == 0;
                });
                if (found == friends_.end()) {
                    if (friends_.size() >= MAX_INET_DEVICE_AROUND) break;
                    friends_.push_back(peer);
                    found = std::prev(friends_.end());
                } else {
                    *found = peer;
                }
            }
            const auto index = static_cast<std::uint32_t>(found - friends_.begin());
            friend_device_address_mapping_[found->dvc_addr_] = index;
            if (current_active_observer_ && !found->refreshed_) {
                found->refreshed_ = true;
                current_active_observer_->on_stranger_call(found->real_addr_, index);
            }
        }
        friend_info_cached_ = true;
    }

    void midman_inet::setup_lan_discovery() {
        mdns_ = std::make_unique<mdns_discovery>(random_device_addr_, password_,
            static_cast<std::uint16_t>(port_), [this]() { sync_lan_friends(); });
    }
}
