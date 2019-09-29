/*
 * Copyright (c) 2019 EKA2L1 Team
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
#include <array>

#include <epoc/services/hwrm/light/light_def.h>

namespace eka2l1 {
    namespace service {
        class property;
    }

    class kernel_system;

    namespace epoc::hwrm::light {
        /**
         * \brief Short brief info about a light.
         */
        struct target_info {
            std::uint32_t target_;      ///< The target of this light.
            std::uint32_t status_;      ///< The status of the light.
        };

        struct resource_data {
            std::array<target_info, MAXIMUM_LIGHT> infos_;
            service::property *infos_prop_;

        public:
            explicit resource_data(kernel_system *kern);

            /**
             * \brief Publish new light status.
             * 
             * \param light_target The target of the light to change.
             * \param sts          The status of the light.
             */
            void publish_light(target light_target, status sts);
        };
    }
}