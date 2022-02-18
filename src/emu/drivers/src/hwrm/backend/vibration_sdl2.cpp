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

#include <drivers/hwrm/backend/vibration_sdl2.h>
#include <SDL.h>

#include "../../sdl2_scoping.h"

namespace eka2l1::drivers::hwrm {
    vibrator_sdl2::vibrator_sdl2() {
        initialize_sdl2_if_not_yet();

        const int count_haptic = SDL_NumHaptics();
        haptics_.resize(count_haptic);

        for (int i = 0; i < count_haptic; i++) {
            SDL_Haptic *haptic = SDL_HapticOpen(i);
            if (haptic) {
                if (SDL_HapticRumbleInit(haptic) == 0) {
                    haptics_.push_back(haptic);
                    continue;
                }
            }

            SDL_HapticClose(haptic);
        }
    }

    vibrator_sdl2::~vibrator_sdl2() {
        for (std::size_t i = 0; i < haptics_.size(); i++) {
            SDL_HapticClose(haptics_[i]);
        }
    }

    void vibrator_sdl2::vibrate(const std::uint32_t millisecs, const std::int16_t intensity) {
        for (std::size_t i = 0; i < haptics_.size(); i++) {
            SDL_HapticRumblePlay(haptics_[i], (intensity + MAX_INTENSITY) / 200.0f, millisecs);
        }
    }

    void vibrator_sdl2::stop_vibrate() {
        for (std::size_t i = 0; i < haptics_.size(); i++) {
            SDL_HapticStopAll(haptics_[i]);
        }
    }
}
