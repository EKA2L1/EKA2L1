/*
 * Copyright (c) 2026 EKA2L1 Team
 *
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#include <drivers/location/location.h>

#include <memory>

namespace eka2l1::drivers {
    struct location_ios_state;

    class location_driver_ios : public location_driver {
        std::shared_ptr<location_ios_state> state_;

    public:
        explicit location_driver_ios();
        ~location_driver_ios() override;

        void start(location_update_callback callback) override;
        void stop() override;
    };
}
