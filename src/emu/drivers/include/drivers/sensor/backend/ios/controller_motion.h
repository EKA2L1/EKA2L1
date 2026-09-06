// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace eka2l1::drivers {
    class sensor_driver;

    void set_controller_motion_source(sensor_driver *driver, void *controller);
    void set_controller_motion_rotation(sensor_driver *driver, int degrees);
}
