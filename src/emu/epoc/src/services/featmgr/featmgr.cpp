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
#include <epoc/utils/des.h>
#include <epoc/vfs.h>

#include <common/log.h>
#include <common/e32inc.h>

#include <e32err.h>
#include <featureinfo.h>
#include <features.hrh>

namespace eka2l1 {
    featmgr_server::featmgr_server(system *sys)
        : service::server(sys, "!FeatMgrServer", true) {
        REGISTER_IPC(featmgr_server, feature_supported, EFeatMgrFeatureSupported, "FeatMgr::FeatureSupported");
    }

    void featmgr_server::do_feature_scanning(system *sys) {
        // TODO: There is a lot of features.
        // See in here: https://github.com/SymbianSource/oss.FCL.sf.os.deviceplatformrelease/blob/master/foundation_system/sf_config/config/inc/publicruntimeids.hrh
        
        // 1. We always welcome rendering with OpenGL ES
        enable_features.push_back(KFeatureIdOpenGLES3DApi);

        // 2. Are we welcoming SVG? Check for OpenVG, cause it should be there if this feature is available
        if (sys->get_io_system()->exist(u"z:\\sys\\bin\\libopenvg.dll")) {
            enable_features.push_back(KFeatureIdSvgt);
        }

        // 3. Check for system language. User have responsibility to be honest :D
        // I like automatic detection, but it's not really easy thogh
        switch (sys->get_system_language()) {
        case language::zh: {
            enable_features.push_back(KFeatureIdChinese);
            break;
        }

        case language::jp: {
            enable_features.push_back(KFeatureIdJapanese);
            break;
        }

        case language::ko: {
            enable_features.push_back(KFeatureIdKorean);
            break;
        }

        case language::th: {
            enable_features.push_back(KFeatureIdThai);
            break;
        }

        default:
            break;
        }
        
        // 4: Add stuff you like here.
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

                if (temp_entry.info > 0) {
                    enable_features.push_back(temp_entry.uid);
                }
            }
        }

        {
            featmgr_config_range temp_range;

            for (uint32_t i = 0; i < header.num_range; i++) {
                cfg_file->read_file(&temp_range, 1, sizeof(featmgr_config_entry));
                enable_feature_ranges.push_back(temp_range);
            }
        }

        return true;
    }

    void featmgr_server::feature_supported(service::ipc_context &ctx) {
        if (!config_loaded) {
            bool succ = load_featmgr_configs(ctx.sys->get_io_system());

            if (!succ) {
                LOG_ERROR("Error loading feature manager server config!");
            }

            do_feature_scanning(ctx.sys);
            std::sort(enable_features.begin(), enable_features.end());

            config_loaded = true;
        }

        // NOTE: Newer version of this server use TFeatureEntry struct. Care about this note when this server
        // mess things up.
        const epoc::uid feature_id = *ctx.get_arg<epoc::uid>(0);
        int result = 0;

        // Search for the feature, first in feature list
        if (std::binary_search(enable_features.begin(), enable_features.end(), feature_id)) {
            result = 1;
        } else {
            // Failed? Search in the range.
            // TODO: We can probably improve this with a binary search, which blocks the head and the tail.
            for (const auto &feature_range: enable_feature_ranges) {
                if (feature_range.low_uid <= feature_id && feature_id <= feature_range.high_uid) {
                    result = 1;
                    break;
                }
            }
        }

        ctx.write_arg_pkg(1, result);
        ctx.set_request_status(KErrNone);
    }
}
