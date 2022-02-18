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

#include <drivers/hwrm/vibration.h>
#include <SDL_haptic.h>
#include <vector>

namespace eka2l1::drivers::hwrm {
    class vibrator_sdl2 : public vibrator {
        std::vector<SDL_Haptic*> haptics_;

    public:
        explicit vibrator_sdl2();
        ~vibrator_sdl2() override;

        void vibrate(const std::uint32_t millisecs, const std::int16_t intensity = MIN_INTENSITY - 1) override;
        void stop_vibrate() override;
    };
}