/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/hwrm/vibration/vibration_def.h>

namespace eka2l1 {
    namespace service {
        class property;
    }

    struct central_repo;
    class io_system;

    namespace manager {
        class device_manager;
    }

    using property_ptr = service::property *;

    class kernel_system;

    namespace epoc::hwrm::vibration {
        struct resource_data {
        private:
            property_ptr status_prop_; ///< Pointer to the property
            central_repo *vibra_control_repo_;

            /**
             * \brief   Initialise internal vibration resource components.
             * 
             * This include:
             * - Vibration status property: Provide way for clients to know status of vibration at get time.
             * 
             * \param   kern Pointer to kernel state.
             * \returns True on success.
             * 
             * \internal
             */
            bool initialise_components(kernel_system *kern, io_system *io, manager::device_manager *mngr);

            /**
             * \brief Enable vibration to start.
             * 
             * This changes the vibration control variable in central repo to true.
             * 
             * \returns True on success.
             * \internal
             */
            bool enable_vibration(io_system *io, manager::device_manager *mngr);

        public:
            explicit resource_data(kernel_system *kern, io_system *io, manager::device_manager *mngr);
        };
    }
}