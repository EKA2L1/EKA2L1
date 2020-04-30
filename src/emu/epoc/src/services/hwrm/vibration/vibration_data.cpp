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

#include <epoc/kernel.h>
#include <epoc/services/hwrm/vibration/vibration_def.h>
#include <epoc/services/hwrm/vibration/vibration_data.h>
#include <epoc/services/hwrm/def.h>
#include <epoc/services/property.h>

namespace eka2l1::epoc::hwrm::vibration {
    bool resource_data::initialise_components(kernel_system *kern) {
        // Create and define the property. Remember to destroy later.
        status_prop_ = kern->create<service::property>();

        if (!status_prop_) {
            LOG_ERROR("Failed to create light service's status property! Abort.");
            return false;
        }

        // Set the category and key, then define
        status_prop_->first = eka2l1::epoc::hwrm::SERVICE_UID;
        status_prop_->second = eka2l1::epoc::hwrm::vibration::VIBRATION_STATUS_KEY;

        // Define and allocate the size that fit our maximum need.
        status_prop_->define(service::property_type::int_data, sizeof(std::uint32_t));
        status_prop_->set_int(static_cast<int>(status_stopped));

        // We done all things good now. Return.
        return true;
    }

    resource_data::resource_data(kernel_system *sys) {
        if (!initialise_components(sys)) {
            LOG_ERROR("Unable to initialise light resource data!");
        }
    }
}