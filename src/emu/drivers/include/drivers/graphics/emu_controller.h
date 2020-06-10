/*
 * Copyright (c) 2018 EKA2L1 Team.
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
#include <functional>
#include <memory>

namespace eka2l1 {
    namespace drivers {
        /**
         * \brief An abstract class to pull controller inputs.
         */
        class emu_controller {
        public:
            virtual ~emu_controller() {}

            virtual void start_polling() = 0;
            virtual void stop_polling() = 0;

            /**
             * \brief Callback on controller button event.
             * 
             * Each controller axis is regarded as two buttons.
             * 
             * \params: controller id.
             * \params: button id.
             * \params: true if the button was pressed, false if release.
             */
            std::function<void(int, int, bool)> on_button_event;
        };

        enum class controller_type {
            glfw
        };

        using emu_controller_ptr = std::unique_ptr<emu_controller>;
        emu_controller_ptr new_emu_controller(controller_type ctl_type);
    }
}
