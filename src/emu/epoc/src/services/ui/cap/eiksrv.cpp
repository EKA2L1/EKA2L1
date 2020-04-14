/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <epoc/kernel.h>
#include <epoc/services/property.h>
#include <epoc/services/ui/cap/eiksrv.h>

#include <array>

namespace eka2l1::epoc::cap {
    enum {
        AVKON_INTERNAL_UID = 0x10207218,
        STATUS_PANE_SYSTEM_DATA_KEY = 0x10000000
    };

    akn_battery_state::akn_battery_state()
        : strength_(BATTERY_LEVEL_MAX)
        , charging_(0)
        , icon_state_(0) {
    }

    akn_signal_state::akn_signal_state()
        : strength_(SIGNAL_LEVEL_MAX)
        , icon_state_(GPRS_SIGNAL_ICON) {
    }

    akn_indicator_state::akn_indicator_state()
        : incall_bubble_flags_(0)
        , incall_bubble_allow_in_usual_(0)
        , incall_bubble_allow_in_idle_(0)
        , incall_bubble_disabled_(0) {
        std::fill(visible_indicators_, visible_indicators_ + MAX_VISIBLE_INDICATORS, 0);
        std::fill(visibile_indicator_states_, visibile_indicator_states_ + MAX_VISIBLE_INDICATORS, 0);
    }

    akn_status_pane_data::akn_status_pane_data()
        : foreground_subscriber_id_(0) {
    }

    eik_status_pane_maintainer::eik_status_pane_maintainer(kernel_system *kern)
        : prop_(nullptr) {
        prop_ = kern->create<service::property>();
        prop_->define(service::property_type::bin_data, sizeof(akn_status_pane_data));

        prop_->first = AVKON_INTERNAL_UID;
        prop_->second = STATUS_PANE_SYSTEM_DATA_KEY;

        // Update data for the first time
        publish_data();
    }

    void eik_status_pane_maintainer::publish_data() {
        prop_->set<akn_status_pane_data>(local_data_);
    }

    bool eik_status_pane_maintainer::set_battery_level(const std::int32_t level) {
        if (level < BATTERY_LEVEL_MIN || level > BATTERY_LEVEL_MAX) {
            return false;
        }

        local_data_.battery_.strength_ = level;
        publish_data();

        return true;
    }

    void eik_status_pane_maintainer::set_battery_charging(const bool is_charging) {
        local_data_.battery_.charging_ = is_charging;
        publish_data();
    }

    bool eik_status_pane_maintainer::set_signal_level(const std::int32_t level) {
        if (level < SIGNAL_LEVEL_MIN || level > SIGNAL_LEVEL_MAX) {
            return false;
        }

        local_data_.signal_.strength_ = level;
        publish_data();

        return true;
    }

    void eik_status_pane_maintainer::set_signal_icon(const std::int32_t icon) {
        local_data_.signal_.icon_state_ = icon;
        publish_data();
    }

    std::int32_t eik_status_pane_maintainer::get_battery_level() const {
        return local_data_.battery_.strength_;
    }

    bool eik_status_pane_maintainer::get_battery_charging() const {
        return local_data_.battery_.charging_;
    }

    std::int32_t eik_status_pane_maintainer::get_signal_level() const {
        return local_data_.signal_.strength_;
    }

    eik_server::eik_server(kernel_system *kern)
        : status_pane_maintainer_(kern) {
    }
}