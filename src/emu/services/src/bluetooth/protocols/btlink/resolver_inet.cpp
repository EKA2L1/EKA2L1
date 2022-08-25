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
        , need_name_(false)
        , in_completion_(false)
        , no_more_friends_(false)
        , in_async_searching_(false)
        , delay_emu_evt_(-1) {
    }

    btlink_inet_host_resolver::~btlink_inet_host_resolver() {
        if (!friend_retrieve_info_.empty()) {
            if (in_completion_) {
                ntimer *timing = friend_retrieve_info_.requester->get_kernel_object_owner()->get_ntimer();
                timing->unschedule_event(delay_emu_evt_, reinterpret_cast<std::uint64_t>(this));
            }

            friend_retrieve_info_.complete(epoc::error_cancel);
        }

        midman_inet *midman = reinterpret_cast<midman_inet*>(papa_->get_midman());

        if (midman->get_discovery_mode() > DISCOVERY_MODE_DIRECT_IP) {
            midman->unregister_stranger_call_observer(static_cast<inet_stranger_call_observer*>(this));
        }
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

        if (in_completion_) {
            return;
        }

        midman_inet *midman = reinterpret_cast<midman_inet*>(papa_->get_midman());
        current_friend_ = 0;

        if (midman->get_discovery_mode() > DISCOVERY_MODE_DIRECT_IP) {
            searched_list_async_.clear();
            in_async_searching_ = false;
            in_completion_ = false;
            no_more_friends_ = false;

            midman->unregister_stranger_call_observer(static_cast<inet_stranger_call_observer*>(this));
            midman->begin_hearing_stranger_call(static_cast<inet_stranger_call_observer*>(this));
        }

        next(result, info);
    }

    void btlink_inet_host_resolver::get_by_name(epoc::socket::name_entry *supply_and_result, epoc::notify_info &info) {
        LOG_WARN(SERVICE_BLUETOOTH, "Get host by name stubbed to not found");
        info.complete(epoc::error_not_supported);
    }

    void btlink_inet_host_resolver::complete_background_find_device_delay_emulation() {
        if (friend_retrieve_info_.empty()) {
            return;
        }

        kernel_system *kern = friend_retrieve_info_.requester->get_kernel_object_owner();
        kern->lock();

        current_friend_ = 0xFFFFFFFF;

        friend_retrieve_info_.complete(epoc::error_eof);
        in_completion_ = false;

        kern->unlock();
    }
    
    void btlink_inet_host_resolver::on_stranger_call(epoc::socket::saddress &addr, std::uint32_t index_in_list) {        
        const std::lock_guard<std::mutex> guard(lock_);

        if (searched_list_async_.find(index_in_list) == searched_list_async_.end()) {
            searched_list_async_.emplace(index_in_list);
        } else {
            return;
        }

        if (no_more_friends_) {
            return;
        }

        if (!new_friends_wait_.empty() || friend_retrieve_info_.empty() || in_async_searching_) {
            new_friends_wait_.push(std::make_pair(addr, index_in_list));
            return;
        }

        in_async_searching_ = true;
        search_next_friend_and_report_to_client(addr, index_in_list);
    }

    void btlink_inet_host_resolver::report_search_end_to_client() {
        midman_inet *midman = reinterpret_cast<midman_inet*>(papa_->get_midman());
        midman->set_friend_info_cached();

        static const char *FIND_DEVICE_DELAY_EMU_NAME = "FindNextDeviceCompleteDelayEmulation";
        static const std::int64_t FIND_DEVICE_DELAY_EMU_DURATION = 2000000;

        ntimer *timing = friend_retrieve_info_.requester->get_kernel_object_owner()->get_ntimer();

        if (delay_emu_evt_ < 0) {
            delay_emu_evt_ = timing->get_register_event(FIND_DEVICE_DELAY_EMU_NAME);
        }
        
        if (delay_emu_evt_ < 0) {
            delay_emu_evt_ = timing->register_event(FIND_DEVICE_DELAY_EMU_NAME, [](std::uint64_t userdata, std::int64_t late) {
                reinterpret_cast<btlink_inet_host_resolver*>(userdata)->complete_background_find_device_delay_emulation();
            });
        }

        timing->schedule_event(FIND_DEVICE_DELAY_EMU_DURATION, delay_emu_evt_, reinterpret_cast<std::uint64_t>(this));            
        in_completion_ = true;
    }

    void btlink_inet_host_resolver::on_no_more_strangers() {
        no_more_friends_ = true;

        if (friend_retrieve_info_.empty()) {
            return;
        }

        kernel_system *kern = friend_retrieve_info_.requester->get_kernel_object_owner();
        kern->lock();

        report_search_end_to_client();

        kern->unlock();
    }

    void btlink_inet_host_resolver::search_next_friend_and_report_to_client(epoc::socket::saddress &addr, std::uint32_t index) {        
        midman_inet *midman = reinterpret_cast<midman_inet*>(papa_->get_midman());

        inquiry_socket_address &cast_addr = static_cast<inquiry_socket_address&>(friend_name_entry_->addr_);
        inquiry_info *info_addr_in = cast_addr.get_inquiry_info();

        info_addr_in->major_service_class_ = MAJOR_SERVICE_TELEPHONY;
        info_addr_in->major_device_class_ = MAJOR_DEVICE_PHONE;
        info_addr_in->minor_device_class_ = MINOR_DEVICE_PHONE_CECULLAR;
        cast_addr.family_ = BTADDR_PROTOCOL_FAMILY_ID;

        friend_querier_.send_request_with_retries(addr, "a", 1, [this, addr, midman, index](const char *result, const std::int64_t bytes) {
            const bool manual_direct_search = (midman->get_discovery_mode() > DISCOVERY_MODE_DIRECT_IP);

            if (bytes <= 0) {
                manual_direct_search ? current_friend_++ : 0;
                next_impl();
            } else {
                inquiry_socket_address &cast_addr = static_cast<inquiry_socket_address&>(friend_name_entry_->addr_);
                inquiry_info *info_addr_in = cast_addr.get_inquiry_info();

                std::memcpy(&info_addr_in->addr_, result, sizeof(device_address));
                midman->add_device_address_mapping(index, info_addr_in->addr_);

                friend_querier_.send_request_with_retries(addr, "n", 1, [this, manual_direct_search](const char *result, const std::int64_t bytes) {
                    kernel_system *kern = friend_retrieve_info_.requester->get_kernel_object_owner();
                    kern->lock();

                    if (bytes < 0) {
                        friend_name_entry_->name_.assign(nullptr, u"Unknown o_o");
                    } else {
                        std::u16string result_u16 = common::utf8_to_ucs2(std::string(result, bytes));
                        friend_name_entry_->name_.assign(nullptr, result_u16);
                    }
                    
                    manual_direct_search ? current_friend_++ : 0;

                    const std::lock_guard<std::mutex> guard(lock_);
                    in_async_searching_ = false;

                    friend_retrieve_info_.complete(epoc::error_none);

                    kern->unlock();
                });
            }
        });
    }

    void btlink_inet_host_resolver::next_impl() {
        if (in_completion_) {
            return;
        }

        midman_inet *midman = reinterpret_cast<midman_inet*>(papa_->get_midman());
        internet::sinet6_address addr;

        if (midman->get_discovery_mode() <= DISCOVERY_MODE_DIRECT_IP) {
            if (!midman->get_friend_address(current_friend_, addr)) {
                report_search_end_to_client();
                return;
            }

            search_next_friend_and_report_to_client(addr, current_friend_);
        } else {
            if (!new_friends_wait_.empty()) {
                std::pair<epoc::socket::saddress, std::uint32_t> search_friend_req = new_friends_wait_.front();
                new_friends_wait_.pop();

                search_next_friend_and_report_to_client(search_friend_req.first, search_friend_req.second);
                return;
            }

            if (no_more_friends_) {
                report_search_end_to_client();
                return;
            }
        }
    }

    void btlink_inet_host_resolver::next(epoc::socket::name_entry *result, epoc::notify_info &info) {        
        const std::unique_lock<std::mutex> guard(lock_);

        if (!friend_retrieve_info_.empty()) {
            info.complete(epoc::error_server_busy);
            return;
        }

        if (current_friend_ == 0xFFFFFFFF) {
            info.complete(epoc::error_not_ready);
            return;
        }

        friend_retrieve_info_ = info;
        friend_name_entry_ = result;

        next_impl();
    }
}
