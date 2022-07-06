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

#include <services/socket/common.h>
#include <cstdint>

namespace eka2l1::epoc::bt {
    enum hci_scan_enable_ioctl_val {
        HCI_SCAN_DISABLED = 0,
        HCI_INQUIRY_SCAN_ONLY = 1,
        HCI_PAGE_SCAN_ONLY = 2,
        HCI_INQUIRY_AND_PAGE_SCAN = 3
    };

    enum {
        BTADDR_PROTOCOL_FAMILY_ID = 0x0101,
        BTLINK_MANAGER_PROTOCOL_ID = 0x0099,
        L2CAP_PROTOCOL_ID = 0x0100,
        SDP_PROTOCOL_ID = 0x0001,
        RFCOMM_PROTOCOL_ID = 0x0003,

        SOL_BT_BLOG = 0x1000,
        SOL_BT_HCI = 0x1010,
        SOL_BT_LINK_MANAGER = 0x1011,
        SOL_BT_L2CAP = 0x1012,
        SOL_BT_RFCOMM = 0x1013,

        HCI_READ_SCAN_ENABLE = 14,
        HCI_WRITE_SCAN_ENABLE = 15,

        LINK_MANAGER_DISCONNECT_ALL_ACL_CONNECTIONS = 0
    };

    struct device_address {
        std::uint8_t addr_[6];
        std::uint16_t padding_;
    };

    static_assert(sizeof(device_address) == 8);

#pragma pack(push, 1)
    struct access_requirements {
        std::uint8_t requirements_;
        std::uint32_t passkey_min_length_;
    };
#pragma pack(pop)

    struct service_security {
        std::uint32_t uid_;
        access_requirements requirements_;
    };

    struct socket_device_address : public socket::saddress {
        static constexpr std::uint32_t DATA_LEN = 8 + sizeof(service_security);

        service_security *get_service_security() {
            return reinterpret_cast<service_security*>(user_data_ + sizeof(device_address));
        }

        device_address *get_device_address() {
            return reinterpret_cast<device_address*>(user_data_);
        }
    };

    enum inquiry_action {
        INQUIRY_ACTION_INCLUDE_ADDR = 1,
        INQUIRY_ACTION_INCLUDE_NAME = 2
    };

    enum major_service_class {
        MAJOR_SERVICE_LIMITED_DISCOVERY_MODE = 1,
        MAJOR_SERVICE_POSITIONING = 8,
        MAJOR_SERVICE_NETWORKING = 0x10,
        MAJOR_SERVICE_RENDERING = 0x20,
        MAJOR_SERVICE_RECORDING = 0x40,
        MAJOR_SERVICE_OBJECT_TRANSFER = 0x80,
        MAJOR_SERVICE_AUDIO = 0x100,
        MAJOR_SERVICE_TELEPHONY = 0x200,
        MAJOR_SERVICE_INFORMATION = 0x400
    };

    enum major_device_class {
        MAJOR_DEVICE_MISC = 0x0,
        MAJOR_DEVICE_COMPUTER = 0x1,
        MAJOR_DEVICE_PHONE = 0x2,
        MAJOR_DEVICE_LAN_ACCESS_POINT = 0x3,
        MAJOR_DEVICE_AV = 0x4,
        MAJOR_DEVICE_PERIPHERAL = 0x5,
        MAJOR_DEVICE_IMAGING = 0x6,
        MAJOR_DEVICE_WEARABLE = 0x7,
        MAJOR_DEVICE_TOY = 0x8
    };

    enum minor_device_class_phone {
        MINOR_DEVICE_PHONE_UNCLASSIFIED = 0x0,
        MINOR_DEVICE_PHONE_CECULLAR = 0x1,
        MINOR_DEVICE_PHONE_CORDLESS = 0x2,
        MINOR_DEVICE_PHONE_SMARTPHONE = 0x3
    };

#pragma pack(push, 1)
    struct inquiry_info {
        device_address addr_;
        std::uint8_t format_type_field_;
        std::uint8_t padding_;
        std::uint16_t major_service_class_;
        std::uint8_t major_device_class_;
        std::uint8_t minor_device_class_;
        std::uint32_t iac_;
        std::uint8_t action_flags_;
        std::uint8_t result_flags_;
        std::int8_t rssi_;
    };
#pragma pack(pop)
    
    struct inquiry_socket_address : public socket::saddress {
        inquiry_info *get_inquiry_info() {
            return reinterpret_cast<inquiry_info*>(user_data_);
        }
    };
}