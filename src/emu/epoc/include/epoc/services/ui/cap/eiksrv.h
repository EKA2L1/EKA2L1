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

#pragma once

#include <cstdint>

// Forward declaration
namespace eka2l1 {
    class kernel_system;
    class window_server;

    namespace service {
        class property;
    }
}

namespace eka2l1::epoc::cap {
    enum {
        BATTERY_LEVEL_MAX = 7,
        BATTERY_LEVEL_MIN = 0,
        SIGNAL_LEVEL_MAX = 7,
        SIGNAL_LEVEL_MIN = 0,

        GPRS_SIGNAL_ICON = 0x4,
        COMMON_PACKET_SIGNAL_ICON = 0x104,
        THREEG_SIGNAL_ICON = 0x204
    };

    struct akn_battery_state {
        std::int32_t strength_;
        std::int32_t charging_;
        std::int32_t icon_state_;

        explicit akn_battery_state();
    };

    struct akn_signal_state {
        std::int32_t strength_;
        std::int32_t icon_state_;

        explicit akn_signal_state();
    };

    struct akn_indicator_state {
        enum {
            MAX_VISIBLE_INDICATORS = 10
        };

        std::int32_t visible_indicators_[MAX_VISIBLE_INDICATORS]; ///< Visible indicator UIDs
        std::int32_t visibile_indicator_states_[MAX_VISIBLE_INDICATORS];
        std::int32_t incall_bubble_flags_;
        bool incall_bubble_allow_in_usual_;
        bool incall_bubble_allow_in_idle_;
        bool incall_bubble_disabled_;

        explicit akn_indicator_state();
    };

    struct akn_status_pane_data {
        std::int32_t foreground_subscriber_id_;
        akn_battery_state battery_;
        akn_signal_state signal_;
        akn_indicator_state indicator_;

        explicit akn_status_pane_data();
    };

    struct eik_status_pane_maintainer {
        service::property *prop_; ///< The property contains status pane data.
        akn_status_pane_data local_data_; ///< Local status pane data.

        explicit eik_status_pane_maintainer(kernel_system *kern);

        void publish_data();

        bool set_battery_level(const std::int32_t level);
        void set_battery_charging(const bool is_charging);

        bool set_signal_level(const std::int32_t level);
        void set_signal_icon(const std::int32_t icon);

        std::int32_t get_battery_level() const;
        bool get_battery_charging() const;
        std::int32_t get_signal_level() const;
        std::int32_t get_signal_icon() const;
    };

    class eik_server {
        eik_status_pane_maintainer status_pane_maintainer_;
        window_server *winserv_;

        enum {
            FLAG_KEY_BLOCK_MODE = 1 << 0
        };

        std::uint32_t flags_;

    public:
        explicit eik_server(kernel_system *kern);
        void init(kernel_system *kern);

        void key_block_mode(const bool is_on);
        const bool key_block_mode() const;

        eik_status_pane_maintainer *get_pane_maintainer() {
            return &status_pane_maintainer_;
        }
    };
};
