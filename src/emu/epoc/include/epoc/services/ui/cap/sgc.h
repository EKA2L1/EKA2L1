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

#include <common/bitfield.h>
#include <epoc/utils/uid.h>

// Forward declare
namespace eka2l1 {
    class kernel_system;

    namespace service {
        class property;
    }

    namespace epoc {
        struct window_group;
    }

    namespace drivers {
        class graphics_driver;
    }

    class window_server;
}

namespace eka2l1::epoc::cap {
    static constexpr epoc::uid UIKON_UID = 0x101F8773;
    static constexpr std::uint32_t UIK_PREFERRED_ORIENTATION_KEY = 9;

    //  System Graphics Coordination
    class sgc_server {
    public:
        struct wg_state {
            using wg_state_flags = common::ba_t<32>;
            std::uint32_t id_;          ///< Id of this window group.

            wg_state_flags flags_;
            std::int32_t sp_layout_;
            std::int32_t sp_flags_;
            std::int32_t app_screen_mode_;

            enum flag {
                FLAG_FULLSCREEN = 0,
                FLAG_PARTIAL_FOREGROUND = 1,
                FLAG_UNDERSTAND_PARTIAL_FOREGROUND = 2,
                FLAG_LEGACY_LAYOUT = 3,
                FLAG_ORIENTATION_SPECIFIED = 4,
                FLAG_ORIENTATION_LANDSCAPE = 5
            };

            void set_fullscreen(const bool set);
            bool is_fullscreen() const;

            void set_legacy_layout(const bool set);
            bool is_legacy_layout() const;

            void set_understand_partial_foreground(const bool set);
            bool understands_partial_foreground() const;

            void set_orientation_specified(const bool set);
            bool orientation_specified() const;

            void set_orientation_landspace(const bool set);
            bool orientation_landscape() const;
        };

    private:
        std::vector<wg_state> states_;

        // Properties
        service::property *orientation_prop_;
        drivers::graphics_driver *graphics_driver_;

        window_server *winserv_;

    public:
        explicit sgc_server();

        bool init(kernel_system *kern, drivers::graphics_driver *driver);
        void change_wg_param(const std::uint32_t id, wg_state::wg_state_flags &flags, const std::int32_t sp_layout,
            const std::int32_t sp_flags, const std::int32_t app_screen_mode);

        void update_screen_state_from_wg(epoc::window_group *group);

        wg_state *get_wg_state(const std::uint32_t wg_id, const bool new_one_if_not_exist);
    };
}