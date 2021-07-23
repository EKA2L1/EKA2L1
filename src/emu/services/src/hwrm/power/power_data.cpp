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

#include <kernel/kernel.h>
#include <services/hwrm/power/power_data.h>
#include <services/hwrm/power/power_def.h>

#include <common/log.h>

namespace eka2l1::epoc::hwrm::power {
    resource_data::resource_data(kernel_system *kern)
        : charging_status_prop_(nullptr)
        , battery_level_prop_(nullptr)
        , battery_status_prop_(nullptr) {
        // Create and define the property. Remember to destroy later.
        charging_status_prop_ = kern->create<service::property>();
        battery_level_prop_ = kern->create<service::property>();
        battery_status_prop_ = kern->create<service::property>();

        if (!charging_status_prop_ || !battery_level_prop_ || !battery_status_prop_) {
            LOG_ERROR(SERVICE_HWRM, "Failed to create power service's properties! Abort.");
            return;
        }

        // Set the category and key, then define
        charging_status_prop_->first = STATE_UID;
        charging_status_prop_->second = CHARGING_STATUS_KEY;

        battery_level_prop_->first = STATE_UID;
        battery_level_prop_->second = BATTERY_LEVEL_KEY;

        battery_status_prop_->first = STATE_UID;
        battery_status_prop_->second = BATTERY_STATUS_KEY;

        // Define and allocate the size that fit our maximum need.
        charging_status_prop_->define(service::property_type::int_data, sizeof(std::uint32_t));
        battery_level_prop_->define(service::property_type::int_data, sizeof(std::uint32_t));
        battery_status_prop_->define(service::property_type::int_data, sizeof(std::uint32_t));

        charging_status_prop_->set_int(static_cast<int>(CHARGING_STATUS_CHARGING));
        battery_level_prop_->set_int(static_cast<int>(BATTERY_LEVEL_MAX));
        battery_status_prop_->set_int(static_cast<int>(BATTERY_STATUS_OK));
    }
}