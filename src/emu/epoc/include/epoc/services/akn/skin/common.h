/*
 * Copyright (c) 2019 EKA2L1 Team.
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
#include <tuple>

namespace eka2l1::epoc {
    enum akn_skin_server_change_handler_notification {
        akn_ssch_content_changed = 1,
        akn_ssch_package_list_updated = 2,
        akn_ssch_config_merged = 3,
        akn_ssch_config_deployed = 4,
        akn_ssch_config_oom = 5,
        akn_ssch_config_corrupt = 6,
        akn_ssch_morphing_state_change = 7,
        akn_ssch_wallpaper_change = 8,
        akn_ssch_flush_cli_sides_cache = 9,
        akn_ssch_anim_background_change = 10,
        akn_ssch_ss_wallpaper_change = 11
    };

    enum {
        AVKON_UID = 0x101F876E,
        PERSONALISATION_UID = 0x101F876F,
        THEMES_UID = 0x102818E8,
        ICON_CAPTION_UID = 0x1028583D
    };
    
    using pid = std::pair<std::uint32_t, std::uint32_t>;
}
