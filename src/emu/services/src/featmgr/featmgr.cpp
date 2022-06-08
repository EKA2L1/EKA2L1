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

#include <common/log.h>

#include <services/context.h>
#include <services/featmgr/featmgr.h>
#include <services/featmgr/op.h>
#include <system/epoc.h>
#include <utils/des.h>
#include <utils/err.h>
#include <vfs/vfs.h>

namespace eka2l1 {
    featmgr_server::featmgr_server(system *sys)
        : service::server(sys->get_kernel_system(), sys, nullptr, "!FeatMgrServer", true) {
        REGISTER_IPC(featmgr_server, feature_supported, EFeatMgrFeatureSupported, "FeatMgr::FeatureSupported");
    }

    enum feature_id : epoc::uid {
        feature_id_opengl_es_3d_api = 10,
        feature_id_svgt = 77,
        feature_id_side_volume_key = 207,
        feature_id_pen = 410,
        feature_id_vibra = 411,
        feature_id_korean = 180,
        feature_id_japanese = 1080,
        feature_id_thai = 1081,
        feature_id_chinese = 1096,
        feature_id_flash_lite_viewer = 1145,
        feature_id_pen_calibration = 1658,
        feature_id_tactile_feedback = 1718
    };

    void featmgr_server::do_feature_scanning(system *sys) {
        // TODO: There is a lot of features.
        // See in here: https://github.com/SymbianSource/oss.FCL.sf.os.deviceplatformrelease/blob/master/foundation_system/sf_config/config/inc/publicruntimeids.hrh

        // 1. We always welcome rendering with OpenGL ES and Flash
        enable_features.push_back(feature_id_opengl_es_3d_api);
        enable_features.push_back(feature_id_flash_lite_viewer);
        enable_features.push_back(feature_id_side_volume_key);
        enable_features.push_back(feature_id_pen);
        enable_features.push_back(feature_id_vibra);
        enable_features.push_back(feature_id_pen_calibration);

        // 2. Are we welcoming SVG? Check for OpenVG, cause it should be there if this feature is available
        if (sys->get_io_system()->exist(u"z:\\sys\\bin\\libopenvg.dll")) {
            enable_features.push_back(feature_id_svgt);
        }

        // 3. Check for system language. User have responsibility to be honest :D
        // I like automatic detection, but it's not really easy thogh
        switch (sys->get_system_language()) {
        case language::zh: {
            enable_features.push_back(feature_id_chinese);
            break;
        }

        case language::jp: {
            enable_features.push_back(feature_id_japanese);
            break;
        }

        case language::ko: {
            enable_features.push_back(feature_id_korean);
            break;
        }

        case language::th: {
            enable_features.push_back(feature_id_thai);
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
            LOG_WARN(SERVICE_FEATMGR, "Feature registration config file not present!");
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
                LOG_ERROR(SERVICE_FEATMGR, "Error loading feature manager server config!");
            }

            do_feature_scanning(ctx.sys);
            std::sort(enable_features.begin(), enable_features.end());

            config_loaded = true;
        }

        epoc::uid feature_id = 0;

        // NOTE: Newer version of this server use TFeatureEntry struct. Care about this note when this server
        // mess things up.
        if (ctx.sys->get_symbian_version_use() >= epocver::epoc95) {
            std::optional<feature_entry> entry = ctx.get_argument_data_from_descriptor<feature_entry>(0);
            if (!entry.has_value()) {
                ctx.complete(epoc::error_argument);
                return;
            }

            feature_id = entry->feature_id_;
        } else {
            feature_id = *ctx.get_argument_value<epoc::uid>(0);
        }

        int result = 0;

        // Search for the feature, first in feature list
        if (std::binary_search(enable_features.begin(), enable_features.end(), feature_id)) {
            result = 1;
        } else {
            // Failed? Search in the range.
            // TODO: We can probably improve this with a binary search, which blocks the head and the tail.
            for (const auto &feature_range : enable_feature_ranges) {
                if (feature_range.low_uid <= feature_id && feature_id <= feature_range.high_uid) {
                    result = 1;
                    break;
                }
            }
        }

        ctx.write_data_to_descriptor_argument(1, result);
        ctx.complete(epoc::error_none);
    }
}
