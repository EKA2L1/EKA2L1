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

#include <epoc/kernel.h>
#include <epoc/services/hwrm/def.h>
#include <epoc/services/hwrm/light/light_data.h>
#include <epoc/services/property.h>

namespace eka2l1::epoc::hwrm::light {
    bool resource_data::initialise_components(kernel_system *kern) {
        // Create and define the property. Remember to destroy later.
        infos_prop_ = kern->create<service::property>();

        if (!infos_prop_) {
            LOG_ERROR("Failed to create light service's status property! Abort.");
            return false;
        }

        // Set the category and key, then define
        infos_prop_->first = eka2l1::epoc::hwrm::SERVICE_UID;
        infos_prop_->second = eka2l1::epoc::hwrm::light::LIGHT_STATUS_PROP_KEY;

        // Define and allocate the size that fit our maximum need.
        infos_prop_->define(service::property_type::bin_data, MAXIMUM_LIGHT * sizeof(target_info));

        // Initialise all the light with default value
        for (std::size_t i = 0; i < infos_.size(); i++) {
            infos_[i].target_ = static_cast<std::uint32_t>(1 << i);
            infos_[i].status_ = epoc::hwrm::light::light_status_off;
        }

        // Publish the default value of the property
        if (!infos_prop_->set(reinterpret_cast<std::uint8_t*>(&infos_[0]), MAXIMUM_LIGHT * sizeof(target_info))) {
            LOG_ERROR("Failed to publish default value of light infos to created property. Abort.");
            return false;
        }

        // We done all things good now. Return.
        return true;
    }

    bool resource_data::publish_light(target light_target, status sts) {
        // Find the light in the list
        auto find_result = std::find_if(infos_.begin(), infos_.end(), [light_target](const target_info &trg) {
            return trg.target_ == light_target;
        });

        if (find_result == infos_.end()) {
            LOG_ERROR("Can't find light target: {}", static_cast<int>(light_target));
            return false;
        }

        // Change our status
        find_result->status_ = sts;

        // Publish the resource to property.
        if (!infos_prop_->set(reinterpret_cast<std::uint8_t*>(&infos_[0]), MAXIMUM_LIGHT * sizeof(target_info))) {
            LOG_ERROR("Failed to publish updated value of light infos to light status property. Abort.");
            return false;
        }

        // We done all things good now. Return. Again.
        return true;
    }
    
    resource_data::resource_data(kernel_system *sys) {
        if (!initialise_components(sys)) {
            LOG_ERROR("Unable to initialise light resource data!");
        }
    }
}