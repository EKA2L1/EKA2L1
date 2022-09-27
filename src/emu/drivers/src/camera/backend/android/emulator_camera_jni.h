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
#include <vector>

namespace eka2l1::drivers::android {
    int emulator_camera_count();
    int emulator_camera_initialize(int index);

    std::vector<int> emulator_camera_get_supported_image_output_formats();

    int emulator_camera_get_flash_mode(int handle);
    bool emulator_camera_set_flash_mode(int handle, int mode);
    bool emulator_camera_is_facing_front(int handle);

    std::vector<eka2l1::vec2> emulator_camera_get_output_image_sizes(int handle);
    bool emulator_camera_capture_image(int handle, int resolution_index, int format);
    bool emulator_camera_receive_viewfinder_feed(int handle, int width, int height, int format);
    void emulator_camera_stop_viewfinder_feed(int handle);
}