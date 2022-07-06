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

#include <common/log.h>
#include <services/bluetooth/btmidman.h>
#include <services/bluetooth/protocols/btlink/btlink_inet.h>
#include <services/bluetooth/protocols/btmidman_inet.h>
#include <services/bluetooth/protocols/common.h>
#include <services/internet/protocols/common.h>

#include <utils/err.h>

#include <kernel/thread.h>
#include <kernel/kernel.h>
#include <utils/err.h>

namespace eka2l1::epoc::bt {
    btlink_inet_host_resolver::btlink_inet_host_resolver(btlink_inet_protocol *papa)
        : papa_(papa)
        , friend_name_entry_(nullptr)
        , current_friend_(0xFFFFFFFF)
        , need_name_(false) {
    }

    btlink_inet_host_resolver::~btlink_inet_host_resolver() {
        friend_retrieve_info_.complete(epoc::error_cancel);
    }

    std::u16string btlink_inet_host_resolver::host_name() const {
        return papa_->get_midman()->device_name();
    }

    bool btlink_inet_host_resolver::host_name(const std::u16string &name) {
        papa_->get_midman()->device_name(name);
        return true;
    }

    void btlink_inet_host_resolver::get_by_address(epoc::socket::saddress &addr, epoc::socket::name_entry *result, epoc::notify_info &info) {
        inquiry_socket_address &cast_addr = static_cast<inquiry_socket_address&>(addr);
        inquiry_info *info_addr_in = cast_addr.get_inquiry_info();

        if (info_addr_in->action_flags_ & INQUIRY_ACTION_INCLUDE_NAME) {
            need_name_ = true;
        }

        current_friend_ = 0;
        next(result, info);
    }

    void btlink_inet_host_resolver::get_by_name(epoc::socket::name_entry *supply_and_result, epoc::notify_info &info) {
        LOG_WARN(SERVICE_BLUETOOTH, "Get host by name stubbed to not found");
        info.complete(epoc::error_not_supported);
    }

    void btlink_inet_host_resolver::next(epoc::socket::name_entry *result, epoc::notify_info &info) {
        if (!friend_retrieve_info_.empty()) {
            info.complete(epoc::error_server_busy);
            return;
        }

        if (current_friend_ == 0xFFFFFFFF) {
            info.complete(epoc::error_not_ready);
            return;
        }

        midman_inet *midman = reinterpret_cast<midman_inet*>(papa_->get_midman());
        internet::sinet6_address addr;

        if (!midman->get_friend_address(current_friend_, addr)) {
            info.complete(epoc::error_eof);
            return;
        }

        friend_retrieve_info_ = info;
        friend_name_entry_ = result;

        inquiry_socket_address &cast_addr = static_cast<inquiry_socket_address&>(result->addr_);
        inquiry_info *info_addr_in = cast_addr.get_inquiry_info();

        info_addr_in->major_service_class_ = MAJOR_SERVICE_TELEPHONY;
        info_addr_in->major_device_class_ = MAJOR_DEVICE_PHONE;
        info_addr_in->minor_device_class_ = MINOR_DEVICE_PHONE_CECULLAR;

        std::uint32_t friend_number = current_friend_ + 1;

        info_addr_in->addr_.addr_[0] = 'E';
        info_addr_in->addr_.addr_[1] = 'K';
        info_addr_in->addr_.addr_[2] = 'A';
        info_addr_in->addr_.addr_[3] = (friend_number >> 16) & 0xFF;
        info_addr_in->addr_.addr_[4] = (friend_number >> 8) & 0xFF;
        info_addr_in->addr_.addr_[5] = friend_number & 0xFF;

        result->addr_.family_ = BTADDR_PROTOCOL_FAMILY_ID;

        if (!need_name_) {
            info.complete(epoc::error_none);
            return;
        }

        friend_querier_.send_request_with_retries(addr, "n", 1, [this](const char *result, const ssize_t bytes) {            
            kernel_system *kern = friend_retrieve_info_.requester->get_kernel_object_owner();
            kern->lock();

            if (bytes < 0) {
                friend_name_entry_->name_.assign(nullptr, u"Offline v_v");
            } else {
                std::u16string result_u16 = common::utf8_to_ucs2(std::string(result, bytes));
                friend_name_entry_->name_.assign(nullptr, result_u16);
            }
            
            current_friend_++;
            friend_retrieve_info_.complete(epoc::error_none);
            kern->unlock();
        });
    }
}