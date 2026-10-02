/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <services/context.h>
#include <services/etel/subsess.h>
#include <utils/err.h>

namespace eka2l1 {
    etel_subsession::etel_subsession(etel_session *session, const etel_legacy_level lvl)
        : session_(session)
        , legacy_level_(lvl) {
    }

    void etel_subsession::dispatch_unhandled(service::ipc_context *ctx) {
        // EIsaCancelMessage for a request absent from the active list is a successful no-op.
        if (ctx->get_argument_value<std::uint32_t>(1) == 5) {
            ctx->complete(epoc::error_none);
            return;
        }

        LOG_ERROR(SERVICE_ETEL, "Unimplemented etel subsession opcode {}", ctx->msg->function);
        ctx->complete(epoc::error_not_supported);
    }
}
