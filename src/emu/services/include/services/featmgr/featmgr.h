/*
 * Copyright (c) 2018 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#include <common/common.h>
#include <kernel/server.h>

#include <vector>

namespace eka2l1 {
    class io_system;

    struct featmgr_config_header {
        char magic[4];
        uint32_t unk1;
        uint32_t num_entry;
        uint32_t num_range;
    };

    struct featmgr_config_entry {
        uint32_t uid;
        uint32_t info;
    };

    struct featmgr_config_range {
        uint32_t low_uid;
        uint32_t high_uid;
    };

    class featmgr_server : public service::server {
        std::vector<epoc::uid> enable_features;
        std::vector<featmgr_config_range> enable_feature_ranges;

        bool config_loaded = false;

        // Load the feature manager config files.
        bool load_featmgr_configs(io_system *io);
        void feature_supported(service::ipc_context &ctx);

        void do_feature_scanning(system *sys);

    public:
        featmgr_server(system *sys);
    };
}
