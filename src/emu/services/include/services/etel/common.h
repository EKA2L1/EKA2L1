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

#include <utils/des.h>

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

    enum etel_legacy_level {
        ETEL_LEGACY_LEVEL_LEGACY,
        ETEL_LEGACY_LEVEL_TRANSITION,
        ETEL_LEGACY_LEVEL_MORDEN
    };

    struct etel_entity {
    public:
        virtual ~etel_entity() = default;
        virtual epoc::etel_entry_type type() const = 0;
    };
}

namespace eka2l1::epoc {
    enum etel_old {
        etel_old_open_from_session = 0,
        etel_old_open_from_subsession = 1,
        etel_old_close = 4,
        etel_old_load_phone_module = 7,
        etel_old_unload_phone_module = 8,
        etel_old_enumerate_phones = 9,
        etel_old_get_phone_info = 10,
        etel_old_set_priority_client = 11,
        etel_old_is_supported_by_module = 12,
        etel_old_get_tsy_name = 13,
        etel_old_phone_get_info = 19,
        etel_old_set_extend_error_granularity = 14,
        etel_old_phone_init = 15,
        etel_old_phone_get_caps = 22,
        etel_old_phone_get_status = 23,
        etel_old_phone_enumerate_lines = 24,
        etel_old_phone_get_line_info = 25,
        etel_old_line_get_info = 26,
        etel_old_line_notify_incoming_call = 27,
        etel_old_line_notify_incoming_call_cancel = 28,
        etel_old_line_notify_hook_change = 29,
        etel_old_line_notify_hook_change_cancel = 30,
        etel_old_line_notify_status_change = 31,
        etel_old_line_notify_status_change_cancel = 32,
        etel_old_line_notify_call_added = 33,
        etel_old_line_notify_call_added_cancel = 34,
        etel_old_line_notify_cap_changes = 35,
        etel_old_line_notify_cap_changes_cancel = 36,
        etel_old_line_get_caps = 37,
        etel_old_line_get_status = 38,
        etel_old_line_get_hook_status = 39,
        etel_old_line_enumerate_call = 40,
        etel_old_line_get_call_info = 41,
        etel_old_call_connect = 49,
        etel_old_call_answer_incoming_call = 50,
        etel_old_call_hang_up = 51,
        etel_old_call_set_fax_setting = 72,
        etel_old_get_tsy_version_number = 77,
        etel_old_gsm_phone_get_phone_id = 1000,
        etel_old_gsm_phone_get_current_network_info = 1027,
        etel_old_gsm_adv_phone_get_subscriber_id = 0x877
    };

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
        etel_close_phone_module = 53,
        etel_get_tsy_name = 55,
        etel_load_phone_module = 57,
        etel_phone_info_by_index = 59,
        etel_query_tsy_functionality = 60,
        etel_line_notify_incoming_call = 71,
        etel_line_cancel_notify_incoming_call = 72,
        etel_phone_init = 75,
        etel_mobile_phone_transition_get_identity_caps = 3015,
        etel_mobile_phone_transition_get_phone_id = 3016,
        etel_mobile_phone_transition_get_subscriber_id = 3017,
        etel_mobile_line_get_mobile_line_status = 20023,
        etel_mobile_line_notify_status_change = 20024,
        etel_mobile_phone_get_battery_info = 20030,
        etel_mobile_phone_get_identity_caps = 20043,
        etel_mobile_phone_get_indicator = 20046,
        etel_mobile_phone_get_indicator_caps = 20047,
        etel_mobile_phone_get_network_caps = 20052,
        etel_mobile_phone_get_network_registration_status = 20054,
        etel_mobile_phone_get_signal_strength = 20060,
        etel_mobile_phone_notify_battery_info_change = 20067,
        etel_mobile_phone_notify_indicator_changes = 20084,
        etel_mobile_phone_notify_network_registration_status_change = 20092,
        etel_mobile_phone_notify_signal_strength_change = 20097,
        // cancel = original + 500
        etel_mobile_line_cancel_notify_status_change = 20524,
        etel_mobile_phone_get_network_registration_status_cancel = 20554,
        etel_mobile_phone_notify_battery_info_change_cancel = 20567,
        etel_mobile_phone_notify_indicator_changes_cancel = 20584,
        etel_mobile_phone_get_home_network = 22004,
        etel_mobile_phone_get_phone_id = 22012,
        etel_mobile_phone_get_subscriber_id = 22017,
        etel_mobile_phone_get_current_network = 26000,
        etel_mobile_phone_notify_current_network_change = 26001,
        etel_mobile_phone_get_current_network_cancel = 26500
    };

    enum etel_network_type : std::uint32_t {
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

    enum etel_mobile_phone_identity_caps {
        etel_mobile_phone_identity_cap_get_manufacturer = 1 << 0,
        etel_mobile_phone_identity_cap_get_model = 1 << 1,
        etel_mobile_phone_identity_cap_get_revision = 1 << 2,
        etel_mobile_phone_identity_cap_get_serialnumber = 1 << 3,
        etel_mobile_phone_identity_cap_get_subscriber_id = 1 << 4
    };

    enum etel_mobile_phone_indicator_caps {
        etel_mobile_phone_indicator_cap_get = 1 << 0,
        etel_mobile_phone_indicator_cap_notify_change = 1 << 1
    };

    enum etel_mobile_phone_network_caps {
        etel_mobile_phone_network_cap_get_registration_status = 1 << 0,
        etel_mobile_phone_network_cap_notify_registration_status = 1 << 1,
        etel_mobile_phone_network_cap_get_current_mode = 1 << 2,
        etel_mobile_phone_network_cap_notify_mode = 1 << 3,
        etel_mobile_phone_network_cap_get_current_network = 1 << 4,
        etel_mobile_phone_network_cap_notify_current_network = 1 << 5,
        etel_mobile_phone_network_cap_get_home_network = 1 << 6,
        etel_mobile_phone_network_cap_get_detected_networks = 1 << 7,
        etel_mobile_phone_network_cap_manual_network_selection = 1 << 8,
        etel_mobile_phone_network_cap_get_nitz_info = 1 << 9,
        etel_mobile_phone_network_cap_notify_nitz_info = 1 << 10
    };

    enum etel_mobile_phone_indicator {
        etel_mobile_phone_indicator_charger_connected = 1 << 0,
        etel_mobile_phone_indicator_network_avail = 1 << 1,
        etel_mobile_phone_indicator_call_in_progress = 1 << 2
    };

    enum etel_mobile_phone_registration_status {
        etel_mobile_phone_not_registered_not_searching,
        etel_mobile_phone_registered_on_home_network,
        etel_mobile_phone_not_registered_searching,
        etel_mobile_phone_registration_denied,
        etel_mobile_phone_unknown,
        etel_mobile_phone_registered_roaming,
        etel_mobile_phone_not_registered_but_available,
        etel_mobile_phone_not_registered_and_not_available
    };

    enum etel_mobile_phone_network_mode {
        etel_mobile_phone_network_mode_unknown,
        etel_mobile_phone_network_mode_unregistered,
        etel_mobile_phone_network_mode_gsm,
        etel_mobile_phone_network_mode_amps
    };

    enum etel_mobile_phone_network_status {
        etel_mobile_phone_network_status_unknown,
        etel_mobile_phone_network_status_available,
        etel_mobile_phone_network_status_current,
        etel_mobile_phone_network_status_forbidden
    };

    enum etel_mobile_phone_network_band_info {
        etel_mobile_phone_band_unknown,
        etel_mobile_phone_800_band_a,
        etel_mobile_phone_800_band_b,
        etel_mobile_phone_800_band_c,
        etel_mobile_phone_1900_band_a,
        etel_mobile_phone_1900_band_b,
        etel_mobile_phone_1900_band_c,
        etel_mobile_phone_1900_band_d,
        etel_mobile_phone_1900_band_e,
        etel_mobile_phone_1900_band_f
    };

    enum etel_phone_network_access_ {
        etel_phone_network_access_unknown,
        etel_phone_network_access_gsm,
        etel_phone_network_access_gsm_compact,
        etel_phone_network_access_utran,
        etel_phone_network_amps_cellular,
        etel_phone_network_cdma_cellular_std_channel,
        etel_phone_network_cdma_cellular_custom_channel,
        etel_phone_network_cdma_amps_cellular,
        etel_phone_network_cdma_pcs_using_blocks,
        etel_phone_network_cdma_pcs_using_network_access_channels,
        etel_phone_network_jtacs_std_channels,
        etel_phone_network_jtacs_custom_channels,
        etel_phone_network_2ghz_band_using_channels,
        etel_phone_network_generic_acq_record_2000_and_95,
        etel_phone_network_generic_acq_record_856,
        etel_phone_network_gsm_and_utran
    };

    enum etel_phone_current_call_status {
        etel_phone_current_call_none,
        etel_phone_current_call_voice,
        etel_phone_current_call_fax,
        etel_phone_current_call_data,
        etel_phone_current_call_altering,
        etel_phone_current_call_ringing,
        etel_phone_current_call_alternating,
        etel_phone_current_call_dialling,
        etel_phone_current_call_answering,
        etel_phone_current_Call_disconnecting
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

    struct etel_multimode_type {
        std::int32_t extension_id_;
    };

    struct etel_old_bsc_network_id {
        std::uint32_t mcc_;
        std::uint32_t mnc_;
    };

    struct etel_old_phone_network_info {
        etel_old_bsc_network_id network_id_;
        etel_mobile_phone_network_status status_;
        epoc::buf_static<char16_t, 20> short_name_;
        epoc::buf_static<char16_t, 30> long_name_;
    };

    struct etel_old_current_phone_network_info {
        etel_old_phone_network_info network_info_;
        std::uint32_t location_area_code_;
        std::uint32_t cell_id_;
    };

    struct etel_phone_network_info : etel_multimode_type {
        etel_mobile_phone_network_mode mode_;
        etel_mobile_phone_network_status status_;
        etel_mobile_phone_network_band_info band_info_;
        epoc::buf_static<char16_t, 4> country_code_;
        epoc::buf_static<char16_t, 8> cdma_sid_;
        epoc::buf_static<char16_t, 8> analog_sid_;
        epoc::buf_static<char16_t, 8> network_id_;
        epoc::buf_static<char16_t, 30> display_tag_;
        epoc::buf_static<char16_t, 10> short_name_;
        epoc::buf_static<char16_t, 20> long_name_;
        etel_phone_network_access_ access_;
        bool hsdpa_available_indicator;
        bool egprs_available_indicator;
    };

    struct etel_phone_location_area : etel_multimode_type {
        bool area_known_;
        std::uint32_t location_area_code_;
        std::uint32_t cell_id_;
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

    enum etel_charger_status {
        etel_charger_status_connected = 0,
        etel_charger_status_disconnected = 1,
        etel_charger_status_not_charging = 2
    };

    enum etel_battery_status {
        etel_battery_status_unknown = 0,
        etel_battery_status_powered_by_battery = 1,
        etel_battery_status_battery_with_external_power = 2,
        etel_battery_status_no_battery = 3,
        etel_battery_power_fault = 4
    };

    enum {
        MAX_MANUFACTURER_ID_LENGTH = 50,
        MAX_MODEL_ID_LENGTH = 50,
        MAX_REVISION_ID_LENGTH = 50,
        MAX_SERIAL_NUMBER_LENGTH = 50
    };

    struct etel_phone_id_v0 {
        epoc::buf_static<char16_t, MAX_MANUFACTURER_ID_LENGTH> manu_;
        epoc::buf_static<char16_t, MAX_MODEL_ID_LENGTH> model_id_;
        epoc::buf_static<char16_t, MAX_REVISION_ID_LENGTH> revision_id_;
        epoc::buf_static<char16_t, MAX_SERIAL_NUMBER_LENGTH> serial_num_;
    };

    static_assert(sizeof(etel_phone_id_v0) == 432);

    struct etel_phone_id_v1 : public etel_multimode_type {
        etel_phone_id_v0 detail_;
    };

    struct etel_battery_info_v1 : public etel_multimode_type {
        etel_battery_status battery_status_;
        std::uint32_t charge_level_;
    };

    static constexpr std::uint32_t ETEL_PHONE_CHARGER_STATUS_UID = 0x100052C9;
    static constexpr std::uint32_t ETEL_PHONE_CURRENT_CALL_UID = 0x100052CB;
    static constexpr std::uint32_t ETEL_PHONE_BATTERY_BARS_UID = 0x100052D3;
    static constexpr std::uint32_t ETEL_PHONE_NETWORK_BARS_UID = 0x100052D4;

    // Check cellularsvr/telephonyserverplugins/common_tsy/commontsy/exportinc/serviceapi/ctsydomainpskeys.h
    static constexpr std::uint32_t ETEL_CALL_INFO_PROP_UID = 0x102029AC;
    static constexpr std::uint32_t ETEL_ADV_SIMC_STATUS_PROP_UID = 0x100052E9;

    static constexpr std::uint32_t ETEL_MAX_BAR_LEVEL = 7;
    static constexpr std::uint32_t ETEL_MIN_BAR_LEVEL = 0;
    static constexpr std::uint32_t ETEL_BAR_MULTIPLIER = 14;
    static constexpr std::uint32_t ETEL_CALL_INFO_CALL_TYPE_KEY = 2;

    enum etel_call_info_prop_call_type {
        ETEL_CALL_INFO_PROP_CALL_UNINIT = -1,
        ETEL_CALL_INFO_PROP_CALL_NONE = 0,
    };
}