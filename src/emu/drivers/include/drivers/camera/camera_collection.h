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

#include <drivers/camera/camera.h>
#include <cstdint>
#include <memory>

namespace eka2l1::drivers::camera {
    class collection {
    public:
        // get_collection() keeps the backend in a unique_ptr<collection> and, on
        // the iOS simulator, reassigns it -- both destroy a derived object
        // through this base pointer.
        virtual ~collection() = default;

        virtual std::uint32_t count() const = 0;
        virtual std::unique_ptr<instance> make_camera(const std::uint32_t camera_index) = 0;
    };

    collection *get_collection();

    // How far, counter-clockwise, a frame that is already upright in the host
    // device's natural orientation still has to turn to be upright in the guest's
    // picture. A backend owns the raw-readout-to-natural step, which is its own
    // business, and adds this on top. The presenter keeps the value current: a
    // guest switches screen mode, and the host interface rotates, while a camera
    // runs.
    //
    // Close to the accelerometer's angle, but not the same one, and the difference
    // is easy to miss because it vanishes in the orientation most testing happens
    // in:
    //
    //   camera        = (mode.rotation - panel_mount) - host_interface_rotation
    //   accelerometer = (mode.rotation - panel_mount) + host_interface_rotation
    //
    // The host terms have opposite signs. Turning the phone counter-clockwise
    // spins the scene clockwise inside a sensor buffer, while the interface
    // counter-rotates the picture to keep it upright for the viewer. The guest
    // terms match: an app that composes for a rotated panel has already laid the
    // frame out for that panel.
    void set_frame_rotation(const int degrees);
    int frame_rotation();
}
