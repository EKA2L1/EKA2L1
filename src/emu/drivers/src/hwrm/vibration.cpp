/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <common/platform.h>
#include <drivers/hwrm/backend/vibration_null.h>
#include <drivers/hwrm/backend/vibration_sdl2.h>
#ifdef EKA2L1_PLATFORM_ANDROID
#include <drivers/hwrm/backend/vibration_jdk.h>
#endif

namespace eka2l1::drivers::hwrm {
    std::unique_ptr<vibrator> make_suitable_vibrator() {
#ifdef EKA2L1_PLATFORM_ANDROID
        return std::make_unique<vibrator_jdk>();
#endif
        return std::make_unique<vibrator_sdl2>();
    }
}