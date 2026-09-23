/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <services/linnea/linnea.h>

#include <common/log.h>
#include <drivers/hwrm/vibration.h>
#include <utils/err.h>

namespace eka2l1 {
    static constexpr std::uint32_t VIBRATOR_PULSE_MS = 100;

    linnea_session::linnea_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver)
        : service::typical_session(svr, client_ss_uid, client_ver) {
    }

    void linnea_session::create_plugin_subsession(service::ipc_context *ctx) {
        const std::optional<std::uint32_t> plugin_id = ctx->get_argument_value<std::uint32_t>(0);
        if (!plugin_id) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (plugin_id.value() != LINNEA_VIBRATOR_PLUGIN_ID) {
            LOG_WARN(SERVICE_UI, "Linnea plugin 0x{:X} is not available", plugin_id.value());
            ctx->complete(epoc::error_not_supported);
            return;
        }

        const std::int32_t handle = next_subsession_handle_++;
        if (!ctx->write_data_to_descriptor_argument<std::int32_t>(3, handle)) {
            ctx->complete(epoc::error_argument);
            return;
        }

        subsessions_.insert(handle);
        ctx->complete(epoc::error_none);
    }

    void linnea_session::vibrator_request(service::ipc_context *ctx) {
        drivers::hwrm::vibrator *vibrator = server<linnea_server>()->vibrator();

        if (ctx->msg->function == linnea_opcode_plugin_request) {
            // The request's pattern words are interpreted by vibplugin on hardware and carry
            // no duration, so each start plays one short pulse.
            if (vibrator) {
                vibrator->vibrate(VIBRATOR_PULSE_MS);
            }
        } else if (vibrator) {
            vibrator->stop_vibrate();
        }

        ctx->complete(epoc::error_none);
    }

    void linnea_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case linnea_opcode_create_plugin_subsession:
            create_plugin_subsession(ctx);
            return;

        case linnea_opcode_close_plugin_subsession:
            subsessions_.erase(ctx->get_argument_value<std::int32_t>(3).value_or(0));
            ctx->complete(epoc::error_none);
            return;

        case linnea_opcode_plugin_request:
        case linnea_opcode_plugin_request_2: {
            if (!subsessions_.count(ctx->get_argument_value<std::int32_t>(3).value_or(0))) {
                ctx->complete(epoc::error_bad_handle);
                return;
            }

            vibrator_request(ctx);
            return;
        }

        case linnea_opcode_unregister:
        case linnea_opcode_plugin_cancel:
            ctx->complete(epoc::error_none);
            return;

        default:
            break;
        }

        LOG_ERROR(SERVICE_UI, "Unimplemented Linnea opcode: {}", ctx->msg->function);
        ctx->complete(epoc::error_not_supported);
    }

    linnea_server::linnea_server(eka2l1::system *sys)
        : service::typical_server(sys, "LinneaApplicationServer")
        , vibrator_(drivers::hwrm::make_suitable_vibrator()) {
    }

    linnea_server::~linnea_server() = default;

    void linnea_server::connect(service::ipc_context &context) {
        create_session<linnea_session>(&context);
        context.complete(epoc::error_none);
    }
}
