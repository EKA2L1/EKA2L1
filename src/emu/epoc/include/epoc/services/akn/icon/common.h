/*
 * Copyright (c) 2002 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project / Symbian Source Code
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

#include <common/vecx.h>

#include <common/rgb.h>
#include <epoc/utils/fscmn.h>
#include <epoc/services/window/common.h>

namespace eka2l1::epoc {
    struct akn_icon_srv_return_data {
        int bitmap_handle;
        int mask_handle;
        eka2l1::vec2 content_dim;
    };

    struct akn_icon_init_data {
        int compression;

        epoc::display_mode icon_mode;
        epoc::display_mode icon_mask_mode;
        epoc::display_mode photo_mode;
        epoc::display_mode video_mode;
        epoc::display_mode offscreen_mode;
        epoc::display_mode offscreen_mask_mode;
    };

    struct akn_icon_params {
        enum flags {
            flag_use_default_icon_dir,
            flag_mbm_icon,
            flag_exclude_from_cache,
            flag_disable_compression
        };

        epoc::filename file_name;
        int bitmap_id;
        int mask_id;
        eka2l1::object_size size;
        int mode;
        int rotation;

        common::rgb color;
        epoc::rfile file_native;
        bool app_icon;

        int flags;
    };
}
