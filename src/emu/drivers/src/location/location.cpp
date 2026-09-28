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

#include <common/platform.h>
#include <drivers/location/location.h>

#if EKA2L1_PLATFORM(ANDROID)
#include <drivers/location/backend/android/location_android.h>
#endif

#if EKA2L1_PLATFORM(IOS)
#include "backend/ios/location_ios.h"
#endif

namespace eka2l1::drivers {
    std::unique_ptr<location_driver> make_location_driver() {
#if EKA2L1_PLATFORM(ANDROID)
        return std::make_unique<location_driver_android>();
#elif EKA2L1_PLATFORM(IOS)
        return std::make_unique<location_driver_ios>();
#else
        return nullptr;
#endif
    }
}
