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

#include <string>
#include <optional>

namespace eka2l1 {
    struct central_repo;
    class central_repo_server;

    class device_manager;
    class io_system;
}

namespace eka2l1::epoc {
    class coe_data_storage {
        eka2l1::central_repo *fep_repo_;
        eka2l1::central_repo_server *serv_;

        device_manager *dmngr_;
        io_system *io_;

    public:
        explicit coe_data_storage(eka2l1::central_repo_server *serv, io_system *io,
            device_manager *mngr);

        eka2l1::central_repo *fep_repo();

        std::optional<std::u16string> default_fep();
        void default_fep(const std::u16string &the_fep);

        void serialize();
    };
}