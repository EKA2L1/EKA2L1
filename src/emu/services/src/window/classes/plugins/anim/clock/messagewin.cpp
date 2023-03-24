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

#include <services/window/classes/plugins/anim/clock/messagewin.h>
#include <services/window/classes/winuser.h>

#include <common/log.h>
#include <utils/err.h>

namespace eka2l1::epoc {
    messagewin_anim_executor::messagewin_anim_executor(canvas_base *canvas)
        : anim_executor(canvas) {
        canvas->set_visible(false);
    }

    std::int32_t messagewin_anim_executor::handle_request(const std::int32_t opcode, void *args)  {
        LOG_TRACE(SERVICE_WINDOW, "Unimplemented message window animation command {}", opcode);
        return epoc::error_none;
    }

    messagewin_anim_executor::~messagewin_anim_executor() {
        canvas_->set_visible(true);
    }
}