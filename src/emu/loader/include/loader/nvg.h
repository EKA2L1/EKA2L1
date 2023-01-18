/*
 * Copyright (c) 2023 EKA2L1 Team
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

#include <common/buffer.h>
#include <cstdint>
#include <string>

namespace eka2l1::loader {
    enum nvg_convert_error {
        NVG_NO_ERROR = 0,
        NVG_NOT_NVG_FILE,
        NVG_END_OF_FILE,
        NVG_READ_COMMAND_DATA_FAILED,
        NVG_UNRECOGNISED_DIRECT_COMMAND_OPCODE,
        NVG_UNRECOGNISED_FILL_PAINT_TYPE,
        NVG_FAILED_TO_WRITE_TO_DEST_FILE,
        NVG_UNKNOWN_PATH_SEGMENT_TYPE,
        NVG_TVL_FORMAT_UNSUPPORTED
    };

    enum nvg_aspect_ratio_mode {
        NVG_PRESERVE_ASPECT_RATIO = 0,
        NVG_PRESERVE_ASPECT_RATIO_AND_REMOVE_UNUSED_SPACE = 1,
        NVG_NOT_PRESERVE_ASPECT_RATIO = 2,
        NVG_PRESERVE_ASPECT_RATIO_SLICE = 3
    };

    struct nvg_options {
        int width = -1;
        int height = -1;

        nvg_aspect_ratio_mode aspect_ratio_mode_ = NVG_PRESERVE_ASPECT_RATIO;
    };

    struct nvg_convert_error_description {
        nvg_convert_error reason_;
        std::uint64_t byte_position_;
        std::uint64_t extra1_;

        nvg_convert_error_description(nvg_convert_error reason, std::uint64_t byte_position)
            : reason_(reason)
            , byte_position_(byte_position) {
        }

        nvg_convert_error_description(nvg_convert_error reason, std::uint64_t byte_position, std::uint64_t extra1)
            : reason_(reason)
            , byte_position_(byte_position)
            , extra1_(extra1) {
        }
    };

    bool convert_nvg_to_svg(common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors,
        nvg_options *options = nullptr);
}