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

#include <common/types.h>
#include <common/common.h>

#include <vector>

namespace eka2l1 {
    class central_repo_server;
    struct central_repo;

    class io_system;

    namespace manager {
        class device_manager;
    }
}

namespace eka2l1::epoc {
    class akn_skin_icon_config_map {
        eka2l1::central_repo_server *cenrep_serv_;
        io_system *io_;
        manager::device_manager *mngr_;

        std::vector<epoc::uid> cfgs_;
        bool inited_ = false;

        language sys_lang_;

    protected:
        /**
         * \brief Read and parse the icon caption central repository.
         */
        void read_and_parse_repo();

    public:
        explicit akn_skin_icon_config_map(central_repo_server *cenrep_,
            manager::device_manager *mngr, io_system *io_, const language lang_);

        /**
         * \brief Check if an icon is already configured (?)
         * 
         * TODO: Find more about what this mean.
         * 
         * \param    app_uid The target UID we want to check.
         * \returns  1 on configured. 0 on not. -1 on cenrep file not present.
         */
        int is_icon_configured(const epoc::uid app_uid);
    };
}
