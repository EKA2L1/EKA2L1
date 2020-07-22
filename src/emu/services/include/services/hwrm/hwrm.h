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

#include <services/framework.h>
#include <services/hwrm/light/light_data.h>
#include <services/hwrm/resource.h>
#include <services/hwrm/vibration/vibration_data.h>

#include <memory>

namespace eka2l1 {
    class hwrm_session : public service::typical_session {
        std::unique_ptr<epoc::resource_interface> resource_;

    public:
        explicit hwrm_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
    };

    class hwrm_server : public service::typical_server {
        std::unique_ptr<epoc::hwrm::light::resource_data> light_data_;
        std::unique_ptr<epoc::hwrm::vibration::resource_data> vibration_data_;

        /**
         * \brief       Initialise all shared service data.
         * \returns     True on success.
         */
        bool init();

    public:
        explicit hwrm_server(system *sys);
        void connect(service::ipc_context &ctx) override;
    };
}