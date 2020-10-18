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

#include <kernel/kernel.h>
#include <services/hwrm/def.h>
#include <services/hwrm/vibration/vibration_data.h>
#include <services/hwrm/vibration/vibration_def.h>
#include <kernel/property.h>

#include <services/centralrepo/centralrepo.h>
#include <services/centralrepo/repo.h>
#include <services/context.h>

namespace eka2l1::epoc::hwrm::vibration {
    bool resource_data::enable_vibration(io_system *io, device_manager *mngr) {
        central_repo_entry *entry = vibra_control_repo_->find_entry(VIBRATION_CONTROL_ENABLE_KEY);

        if (!entry) {
            return false;
        }

        // Enable vibration
        entry->data.intd = 1;
        vibra_control_repo_->write_changes(io, mngr);

        return true;
    }

    bool resource_data::initialise_components(kernel_system *kern, io_system *io, device_manager *mngr) {
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

        central_repo_server *cen_rep_server = reinterpret_cast<central_repo_server *>(
            kern->get_by_name<service::server>(CENTRAL_REPO_SERVER_NAME));

        if (!cen_rep_server) {
            return true;
        }

        vibra_control_repo_ = cen_rep_server->load_repo_with_lookup(io, mngr, VIBRATION_CONTROL_REPO_UID);

        if (vibra_control_repo_) {
            enable_vibration(io, mngr);
        }

        // We done all things good now. Return.
        return true;
    }

    resource_data::resource_data(kernel_system *sys, io_system *io, device_manager *mngr)
        : status_prop_(nullptr)
        , vibra_control_repo_(nullptr) {
        if (!initialise_components(sys, io, mngr)) {
            LOG_ERROR("Unable to initialise light resource data!");
        }
    }
}