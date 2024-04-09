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

#include <mem/ptr.h>
#include <utils/des.h>

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
        ICON_CAPTION_UID = 0x1028583D,
        AKN_SKIN_MAJOR_UID = 0x10005a26,
        AKN_SKIN_ICON_MAJOR_UID = 0x101F84B6,
        AKN_WALLPAPER_FILENAME_ID = 0xC0DEF00D,
    
        // Specifics
        AKN_SKIN_MINOR_WALLPAPER_UID = 0x1180
    };

    using pid = std::pair<std::int32_t, std::int32_t>;

    /**
     * \brief Multi-purpose pointer type for AKN server.
     */
    enum akns_mtptr_type {
        akns_mtptr_type_absolute_ram = 2, /** Pointer is absolute on RAM. */
        akns_mtptr_type_relative_ram = 3, /** Pointer is offset on a base. */
        akns_mtptr_type_absolute_rom = 4 /** Pointer is on ROM. */
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

            return reinterpret_cast<T *>(reinterpret_cast<std::uint8_t *>(base) + address_or_offset_);
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

    struct akns_item_def_v2 : public akns_item_def_v1 {
        std::int32_t next_hash_;
    };

    enum akn_skin_protection_type : std::uint32_t {
        akn_skin_protection_none = 0,
        akn_skin_protection_drm = 1,
        akn_skin_protection_no_rights = 2,
    };

    struct akn_skin_info_package {
        pid pid_;
        pid color_scheme_id_;
        epoc::filename skin_directory_buf; // The directory of the skin package
        epoc::filename skin_ini_file_directory_buf; // The directory containing the skin ini file
        epoc::filename skin_name_buf; // The name of the skin package
        epoc::filename idle_state_wall_paper_image_name; // The name of the idle state wallpaper mbm file
        epoc::filename full_name; // The fully qualified skn-file name
        std::uint32_t is_copyable; // Boolean value specifying if the skin package copyable
        std::uint32_t is_deletable; // Boolean value specifying if the skin package is deletable
        std::uint32_t idle_bg_image_index; // The index of the idle state background image
        akn_skin_protection_type protection_type; // Specifies the DRM protection type in this skin
        std::uint32_t corrupted; // Specifies if the skin is somehow corrupted
        std::uint32_t support_anim_bg;
    };

    static constexpr std::size_t SKIN_INFO_PACKAGE_SIZE_NORMAL = sizeof(akn_skin_info_package);

    // No anim bg
    static constexpr std::size_t SKIN_INFO_PACKAGE_SIZE_NO_ANIM_BG = sizeof(akn_skin_info_package) - 4;

    static_assert(sizeof(akns_item_def_v2) == 24);

    using akns_item_def = akns_item_def_v2;
}
