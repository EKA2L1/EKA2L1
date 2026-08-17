/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <drivers/camera/camera.h>
#include <drivers/camera/camera_collection.h>

#include <memory>

namespace eka2l1::drivers::camera {
    // Test-pattern camera for the iOS simulator, which has no AVCaptureDevice
    // at all: without it CCamera::CamerasAvailable() reports 0 and no guest
    // ever exercises the ECam path. Exposes the same back/front pair a real
    // device does and feeds synthesized frames through the shared iOS pixel
    // conversion, so everything downstream of the frame source is the code a
    // real device runs.
    //
    // Never selected on device builds — see get_collection().
    class collection_simulator : public collection {
    public:
        std::uint32_t count() const override;
        std::unique_ptr<instance> make_camera(const std::uint32_t camera_index) override;
    };
}
