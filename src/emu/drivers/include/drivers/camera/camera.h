/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <common/vecx.h>

#include <cstdint>
#include <vector>
#include <functional>

namespace eka2l1::drivers::camera {
    enum parameter_key {
        PARAMETER_KEY_OPTICAL_ZOOM = 0,
        PARAMETER_KEY_DIGITAL_ZOOM = 1,
        PARAMETER_KEY_CONTRAST = 2,
        PARAMETER_KEY_BRIGHTNESS = 3,
        PARAMETER_KEY_FLASH = 4,
        PARAMETER_KEY_EXPOSURE = 5,
        PARAMETER_KEY_WHITE_BALANCE = 6
    };

    enum frame_format {
        FRAME_FORMAT_MONOCHROME = 0x01,
        FRAME_FORMAT_ARGB_4444 = 0x02,
        FRAME_FORMAT_RGB565 = 0x04,
        FRAME_FORMAT_ARGB8888 = 0x08,
        FRAME_FORMAT_JPEG = 0x10,
        FRAME_FORMAT_FBSBMP_COLOR64K = 0x80,
        FRAME_FORMAT_FBSBMP_COLOR16M = 0x100,
        FRAME_FORMAT_YUV420_INTERLEAVED = 0x0400,
        FRAME_FORMAT_YUV420_PLANAR = 0x0800,
        FRAME_FORMAT_YUV422 = 0x1000,
        FRAME_FORMAT_FBSBMP_COLOR16MU = 0x10000,
    };

    enum flash_mode {
        FLASH_MODE_OFF = 0,
        FLASH_MODE_AUTO = 1,
        FLASH_MODE_FORCED = 2,
        FLASH_MODE_VIDEO_LIGHT = 0x80
    };

    enum direction : std::uint32_t {
        DIRECTION_BACK = 0,
        DIRECTION_FRONT = 1
    };

    // First argument: buffer data, second: size of buffer, third: error code (< 0 = error)
    using camera_capture_image_done_callback = std::function<void(const void*, std::size_t, int)>;
    using camera_wants_new_frame_callback = std::function<bool()>;

#pragma pack(push, 1)
    struct info {
        std::uint8_t hardware_version_major_;
        std::uint8_t hardware_version_minor_;
        std::uint16_t hardware_version_build_;
        std::uint8_t software_version_major_;
        std::uint8_t software_version_minor_;
        std::uint16_t software_version_build_;
        direction camera_direction_;
        std::uint32_t options_supported_ = 0;
        std::uint32_t flash_modes_supported_ = 0;
        std::uint32_t exposure_modes_supported_ = 0;
        std::uint32_t white_balance_modes_supported_ = 0;
        std::int32_t min_zoom_ = 0;
        std::int32_t max_zoom_ = 0;
        std::int32_t max_digital_zoom_ = 0;
        float min_zoom_factor_ = 1.0f;
        float max_zoom_factor_ = 1.0f;
        float max_digital_zoom_factor_ = 1.0f;
        std::int32_t num_image_sizes_supported_ = 0;
        std::uint32_t supported_image_formats_ = 0;
        std::int32_t num_video_frame_rates_supported_ = 0;
        std::uint32_t video_frame_format_supported_ = 0;
        std::int32_t max_frame_per_buffer_supported_ = 1;
        std::int32_t max_buffers_supported_ = 1;
    };
#pragma pack(pop)

    class instance {
    public:
        virtual ~instance() = default;

        virtual bool set_parameter(const parameter_key key, const std::uint32_t value) = 0;
        virtual bool get_parameter(const parameter_key key, std::uint32_t &value) = 0;

        virtual std::vector<frame_format> supported_frame_formats() = 0;
        virtual std::vector<eka2l1::vec2> supported_output_image_sizes(const frame_format frame_format) = 0;

        virtual bool reserve() = 0;
        virtual void release() = 0;

        virtual info get_info() = 0;

        virtual void capture_image(const std::uint32_t resolution_index, const frame_format format,
                                   camera_capture_image_done_callback callback) = 0;
        virtual void receive_viewfinder_feed(const eka2l1::vec2 &size, const frame_format format,
                                             camera_wants_new_frame_callback new_frame_needed_callback,
                                             camera_capture_image_done_callback new_frame_come_callback) = 0;
        virtual void stop_viewfinder_feed() = 0;
    };
}