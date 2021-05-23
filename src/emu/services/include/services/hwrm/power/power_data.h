/*
 * Copyright (c) 2021 EKA2L1 Team
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

namespace eka2l1 {
    class kernel_system;

    namespace service {
        class property;
    }

    using property_ptr = service::property *;

    namespace epoc::hwrm::power {
        struct resource_data {
        private:
            property_ptr charging_status_prop_;
            property_ptr battery_level_prop_;
            property_ptr battery_status_prop_;

        public:
            explicit resource_data(kernel_system *kern);
        };
    }
}