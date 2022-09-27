/*
 * Copyright (c) 2020 EKA2L1 Team.
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
#include <dispatch/def.h>
#include <mem/ptr.h>
#include <utils/des.h>
#include <utils/reqsts.h>

namespace eka2l1::drivers::camera {
    struct info;
}

namespace eka2l1::dispatch {
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_number_of_cameras);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_create, std::int32_t index, drivers::camera::info *cam_info);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_claim, std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_power_on, std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_power_off, std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(void, ecam_release, std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_query_still_image_size, std::uint32_t handle,
                           std::uint32_t format, std::int32_t size_index, eka2l1::vec2 *size);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_take_image, std::uint32_t handle, std::int32_t size_index,
                           std::int32_t format, eka2l1::rect *clip_rect, eka2l1::ptr<epoc::request_status> status);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_receive_image, std::uint32_t handle, std::int32_t *size, void *data_ptr);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_start_viewfinder, std::uint32_t handle, eka2l1::vec2 *size,
                           std::int32_t display_mode, eka2l1::ptr<epoc::request_status> status);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_next_viewfinder_frame, std::uint32_t handle, eka2l1::ptr<epoc::request_status> status);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_stop_viewfinder_frame, std::uint32_t handle);
}