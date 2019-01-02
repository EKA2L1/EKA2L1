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

#include <epoc/epoc.h>
#include <epoc/services/featmgr/featmgr.h>
#include <epoc/services/featmgr/op.h>

#include <common/log.h>
#include <epoc/utils/des.h>

#include <epoc/vfs.h>

namespace eka2l1 {
    featmgr_server::featmgr_server(system *sys)
        : service::server(sys, "!FeatMgrServer", true) {
        REGISTER_IPC(featmgr_server, feature_supported, EFeatMgrFeatureSupported, "FeatMgr::FeatureSupported");
    }

    bool featmgr_server::load_featmgr_configs(io_system *io) {
        symfile cfg_file = io->open_file(u"Z:\\private\\102744CA\\featreg.cfg", READ_MODE | BIN_MODE);

        if (!cfg_file) {
            return false;
        }

        featmgr_config_header header;
        cfg_file->read_file(&header, 1, sizeof(featmgr_config_header));

        // Check magic header
        if (strncmp(header.magic, "feat", 4) != 0) {
            return false;
        }

        {
            featmgr_config_entry temp_entry;

            for (uint32_t i = 0; i < header.num_entry; i++) {
                cfg_file->read_file(&temp_entry, 1, sizeof(featmgr_config_entry));
                features.emplace(temp_entry.uid, 1);
            }
        }

        {
            featmgr_config_range temp_range;

            for (uint32_t i = 0; i < header.num_range; i++) {
                cfg_file->read_file(&temp_range, 1, sizeof(featmgr_config_entry));

                for (uint32_t j = temp_range.low_uid; j <= temp_range.high_uid; j++) {
                    features.emplace(j, 1);
                }
            }
        }

        return true;
    }

    void featmgr_server::feature_supported(service::ipc_context ctx) {
        /*
        if (!config_loaded) {
            bool succ = load_featmgr_configs(ctx.sys->get_io_system());

            if (!succ) {
                LOG_ERROR("Error loading feature manager server config!");
            } 

            config_loaded = true;
        }

        auto entry = ctx.get_arg_packed<featmgr_config_entry>(0);

        if (!entry) {
            ctx.write_arg_pkg(1, 1);
            ctx.set_request_status(KErrNone);

            return;
        }

        ctx.write_arg_pkg(1, features[entry->uid]);
        ctx.set_request_status(KErrNone);
        */

        ctx.write_arg_pkg(1, 1);
        ctx.set_request_status(KErrNone);
    }
}