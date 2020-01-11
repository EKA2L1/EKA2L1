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

#include <epoc/ptr.h>

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
    
    using pid = std::pair<std::int32_t, std::int32_t>;

    /**
     * \brief Multi-purpose pointer type for AKN server.
     */
    enum akns_mtptr_type {
        akns_mtptr_type_absolute_ram = 2,       /** Pointer is absolute on RAM. */
        akns_mtptr_type_relative_ram = 3,       /** Pointer is offset on a base. */
        akns_mtptr_type_absolute_rom = 4        /** Pointer is on ROM. */
    };

    /**
     * \brief Multi-purpose pointer for AKN server.
     */
    struct akns_mtptr {
        akns_mtptr_type type_;
        std::uint32_t address_or_offset_;

        template <typename T>
        eka2l1::ptr<T> get(const eka2l1::ptr<T> base) {
            if (type_ == akns_mtptr_type_relative_ram) {
                return base + address_or_offset_;
            }

            return eka2l1::ptr<T>(address_or_offset_);
        }

        template <typename T>
        T *get_relative(void *base) {
            if (type_ != akns_mtptr_type_relative_ram) {
                return nullptr;
            }

            return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(base) + address_or_offset_);
        }
    };

    enum akns_item_type {
        akns_item_type_unknown = 0,
        akns_item_type_bitmap = 1,
        akns_item_type_masked_bitmap = 2,
        akns_item_type_color_table = 3,
        akns_item_type_image_table = 4,
        akns_item_type_image = 5,
        akns_item_type_bmp_anim = 6,
        akns_item_type_string = 7,
        akns_item_type_effect_queue = 8,
        akns_item_type_anim = 9
    };

    struct akns_item_def_v1 {
        pid id_;
        akns_item_type type_;
        akns_mtptr data_;
    };

    struct akns_item_def_v2: public akns_item_def_v1 {
        std::int32_t next_hash_;
    };

    using akns_item_def = akns_item_def_v2;
}
