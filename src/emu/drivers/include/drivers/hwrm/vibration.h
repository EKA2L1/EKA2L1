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

#pragma once

#include <cstdint>
#include <memory>

namespace eka2l1::drivers::hwrm {
    static constexpr std::int32_t MIN_INTENSITY = -100;
    static constexpr std::int32_t MAX_INTENSITY = 100;

    class vibrator {
    public:
        virtual ~vibrator() {
        }

        /**
         * @brief Vibrate the phone for defined duration.
         *
         * @param millisecs     The duration to vibrate.
         * @param intensity     Intensity of the vibration. Range from MIN_INTENSITY to MAX_INTENSITY.
         *                      Falling out of that range will cause the vibration to vibrates with default intensity.
         */
        virtual void vibrate(const std::uint32_t millisecs, const std::int16_t intensity = MIN_INTENSITY - 1) = 0;
    };

    std::unique_ptr<vibrator> make_suitable_vibrator();
};