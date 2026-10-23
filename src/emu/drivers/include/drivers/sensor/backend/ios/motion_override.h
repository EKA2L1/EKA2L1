// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace eka2l1::drivers {
    class sensor_driver;

    // Replaces every motion source with a fixed acceleration in the emulated device's
    // own axes and the Symbian convention (m/s^2, flat face up = +9.81 z). nullptr clears.
    void set_motion_acceleration_override(sensor_driver *driver, const double *acceleration_ms2);
}
