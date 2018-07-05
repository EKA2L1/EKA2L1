#include <core.h>
#include <services/featmgr/featmgr.h>
#include <services/featmgr/op.h>

#include <common/log.h>
#include <epoc/des.h>

namespace eka2l1 {
    featmgr_server::featmgr_server(system *sys)
        : service::server(sys, "!FeatMgrServer") {
        bool succ = load_featmgr_configs(sys->get_io_system());

        if (!succ) {
            LOG_ERROR("Error loading feature manager server config!");
        }

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
        auto entry = ctx.get_arg_packed<featmgr_config_entry>(0);

        if (!entry) {
            ctx.set_request_status(KErrArgument);
        }

        ctx.set_request_status(features[entry->uid]);
    }
}