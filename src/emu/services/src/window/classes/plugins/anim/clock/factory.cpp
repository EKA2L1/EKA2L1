/*
 * Copyright (c) 2023 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
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

#include <services/window/classes/plugins/anim/clock/factory.h>
#include <services/window/classes/plugins/anim/clock/clock.h>
#include <services/window/classes/plugins/anim/clock/messagewin.h>

namespace eka2l1::epoc {
    enum clock_anim_type {
        CLOCK_ANIM_TYPE_CLOCK = 0,
        CLOCK_ANIM_TYPE_MSG_WINDOW = 1
    };

    std::unique_ptr<anim_executor> clock_anim_executor_factory::new_executor(canvas_base *canvas, const std::uint32_t anim_type) {
        switch (anim_type) {
        case CLOCK_ANIM_TYPE_CLOCK:
            return std::make_unique<clock_anim_executor>(canvas);

        case CLOCK_ANIM_TYPE_MSG_WINDOW:
            return std::make_unique<messagewin_anim_executor>(canvas);

        default:
            break;
        }

        return nullptr;
    }
}