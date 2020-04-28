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

#include <epoc/services/etel/line.h>
#include <epoc/services/etel/subsess.h>
#include <epoc/services/context.h>
#include <epoc/utils/err.h>

#include <common/log.h>

namespace eka2l1 {
    etel_line::etel_line(const epoc::etel_line_info &info, const std::string &name, const std::uint32_t caps)
        : info_(info)
        , caps_(caps)
        , name_(name) {
    }

    etel_line_subsession::etel_line_subsession(etel_session *session, etel_line *line)
        : etel_subsession(session)
        , line_(line) {
    }

    void etel_line_subsession::dispatch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::etel_line_get_status:
            get_status(ctx);
            break;

        default:
            LOG_ERROR("Unimplemented etel line opcode {}", ctx->msg->function);
            break;
        }
    }

    void etel_line_subsession::get_status(service::ipc_context *ctx) {
        ctx->write_arg_pkg<epoc::etel_line_status>(0, line_->info_.sts_);
        ctx->set_request_status(epoc::error_none);
    }
}