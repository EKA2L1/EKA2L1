/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <epoc/utils/des.h>

#include <cstdint>
#include <memory>

namespace eka2l1 {
    namespace epoc {        
        enum etel_entry_type {
            etel_entry_call = 0,
            etel_entry_phone = 1,
            etel_entry_line = 2
        };
    }
    
    struct etel_entity {
    public:
        virtual ~etel_entity() = default;
        virtual epoc::etel_entry_type type() const = 0;
    };
}

namespace eka2l1::epoc {
    enum etel_opcode {
        etel_open_from_session = 0,
        etel_open_from_subsession = 1,
        etel_close = 4,
        etel_line_enumerate_call = 34,
        etel_line_get_call_info = 35,
        etel_line_get_status = 39,
        etel_phone_enumerate_lines = 46,
        etel_phone_get_line_info = 49,
        etel_enumerate_phones = 54,
        etel_phone_get_status = 50,
        etel_get_tsy_name = 55,
        etel_load_phone_module = 57,
        etel_phone_info_by_index = 59,
        etel_query_tsy_functionality = 60,
        etel_phone_init = 75,
        etel_mobile_phone_get_indicator = 20046,
        etel_mobile_phone_get_indicators_cap = 20047
    };

    enum etel_network_type: std::uint32_t {
        etel_network_type_wired_analog = 0,
        etel_network_type_wired_digital = 1,
        etel_network_type_mobile_analog = 2,
        etel_network_type_mobile_digital = 3,
        etel_network_type_unk = 4
    };

    enum etel_phone_mode {
        etel_phone_mode_unknown = 0,
        etel_phone_mode_idle = 1,
        etel_phone_mode_establishing_link = 2,
        etel_phone_mode_online_data = 3,
        etel_phone_mode_online_command = 4
    };

    enum etel_modem_detection {
        etel_modem_detect_present = 0,
        etel_modem_detect_not_present = 1,
        etel_modem_detect_unk = 2
    };

    enum etel_line_status {
        etel_line_status_unknown = 0,
        etel_line_status_idle = 1,
        etel_line_status_dialling = 2,
        etel_line_status_ringing = 3,
        etel_line_status_answering = 4,
        etel_line_status_connecting = 5,
        etel_line_status_connected = 6,
        etel_line_status_hanging_up = 7
    };

    enum etel_line_caps {
        etel_line_caps_data = 1 << 0,
        etel_line_caps_fax = 1 << 1,
        etel_line_caps_voice = 1 << 2,
        etel_line_caps_dial = 1 << 3
    };

    enum etel_line_hook_sts {
        etel_line_hook_sts_off = 0,
        etel_line_hook_sts_on = 1,
        etel_line_hook_sts_unknown = 2
    };

    enum etel_mobile_phone_indicator_caps {
        etel_mobile_phone_indicator_cap_get = 1 << 0,
        etel_mobile_phone_indicator_cap_notify_change = 1 << 1
    };

    enum etel_mobile_phone_indicator {
        etel_mobile_phone_indicator_charger_connected = 1 << 0,
        etel_mobile_phone_indicator_network_avail = 1 << 1,
        etel_mobile_phone_indicator_call_in_progress = 1 << 2
    };

    struct etel_phone_status {
        etel_phone_mode mode_;
        etel_modem_detection detect_;
    };

    struct etel_phone_info {
        etel_network_type network_;
        epoc::name phone_name_;
        std::uint32_t line_count_;
        std::uint32_t exts_;
    };

    struct etel_line_info {
        etel_line_hook_sts hook_sts_;
        etel_line_status sts_;
        epoc::name last_call_added_;
        epoc::name last_call_answering_;
    };

    struct etel_line_info_from_phone {
        etel_line_status sts_;
        std::uint32_t caps_;
        epoc::name name_;
    };
    
    struct etel_module_entry {
        std::string tsy_name_;
        std::unique_ptr<etel_entity> entity_;
    };
}