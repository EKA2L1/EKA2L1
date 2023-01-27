/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include "sdl2_scoping.h"
#include <common/log.h>
#include <common/platform.h>
#include <SDL.h>

namespace eka2l1::drivers {
    struct sdl2_system_scoping {
        explicit sdl2_system_scoping() {
            // Who enable this as default? cmonBruh
            SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0");
            SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        
            if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0) {
                LOG_ERROR(DRIVER_GRAPHICS, "Fail to initialize SDL2 system!");
                return;
            }
        }

        ~sdl2_system_scoping() {
            // TODO: Find out why scoping is crashing on Mac
            // Maybe because it's in destructor instead of main() ?
#if !EKA2L1_PLATFORM(DARWIN)
            SDL_Quit();
#endif
        }
    };

    std::unique_ptr<sdl2_system_scoping> scoper = nullptr;

    void initialize_sdl2_if_not_yet() {
        if (!scoper) {
            scoper = std::make_unique<sdl2_system_scoping>();
        }
    }
}
