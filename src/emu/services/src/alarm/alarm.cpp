/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/alarm/alarm.h>
#include <system/epoc.h>

#include <utils/err.h>

namespace eka2l1 {
    static void populate_alarm_ids(common::chunkyseri &seri, std::vector<std::int32_t> &alarm_ids) {
        std::uint32_t property_count = static_cast<std::uint32_t>(alarm_ids.size());
        seri.absorb(property_count);

        for (size_t i = 0; i < property_count; i++) {
            seri.absorb(alarm_ids[i]);
        }
    }

    alarm_server::alarm_server(eka2l1::system *sys)
        : service::typical_server(sys, "!AlarmServer") {
    }

    void alarm_server::connect(service::ipc_context &context) {
        create_session<alarm_session>(&context);
        context.complete(epoc::error_none);
    }

    alarm_session::alarm_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , change_alarm_id_fill(nullptr) {
        transfer_buf.reserve(100);
    }

    void alarm_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        // All three list requests stream the id array into the transfer buffer and hand
        // its size back in slot 1. Their filter argument only narrows a queue that is
        // always empty here, so one implementation answers all of them.
        case alarm_get_alarm_id_list_for_category:
        case alarm_get_alarm_id_list_by_state:
        case alarm_get_alarm_id_list:
            stream_alarm_id_list(ctx);
            break;

        case alarm_fetch_transfer_buffer:
            fetch_transfer_buffer(ctx);
            break;

        case alarm_notify_change:
            notify_change(ctx);
            break;

        case alarm_notify_change_cancel:
            notify_change_cancel(ctx);
            break;

        default:
            LOG_ERROR(SERVICE_ALARM, "Unimplemented opcode for Alarm server 0x{:X}", ctx->msg->function);
            break;
        }
    }

    void alarm_session::stream_alarm_id_list(service::ipc_context *ctx) {
        common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
        populate_alarm_ids(seri, alarm_ids);

        transfer_buf.resize(seri.size());
        seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&transfer_buf[0]), transfer_buf.size(), common::SERI_MODE_WRITE);
        populate_alarm_ids(seri, alarm_ids);

        ctx->write_data_to_descriptor_argument<std::uint32_t>(1, static_cast<std::uint32_t>(transfer_buf.size()));
        ctx->complete(epoc::error_none);
    }

    void alarm_session::fetch_transfer_buffer(service::ipc_context *ctx) {
        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&transfer_buf[0]), static_cast<std::uint32_t>(transfer_buf.size()));
        ctx->complete(epoc::error_none);
    }

    void alarm_session::notify_change(service::ipc_context *ctx) {
        change_alarm_id_fill = ctx->get_descriptor_argument_ptr(0);
        change_notify_info = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void alarm_session::notify_change_cancel(service::ipc_context *ctx) {
        change_notify_info.complete(epoc::error_cancel);
        ctx->complete(epoc::error_none);
    }
}
