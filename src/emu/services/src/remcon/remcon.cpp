/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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
#include <services/remcon/controller.h>
#include <services/remcon/remcon.h>
#include <services/remcon/target.h>
#include <utils/err.h>

namespace eka2l1 {
    remcon_server::remcon_server(eka2l1::system *sys)
        : service::typical_server(sys, "!RemConSrv") {
    }

    void remcon_server::connect(service::ipc_context &ctx) {
        create_session<remcon_session>(&ctx);
        ctx.complete(epoc::error_none);
    }

    remcon_session::remcon_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver)
        : service::typical_session(svr, client_ss_uid, client_ver)
        , type_(epoc::remcon::client_type_undefined) {
    }

    void remcon_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::remcon::remcon_message_set_player_type:
            set_player_type(ctx);
            break;

        case epoc::remcon::remcon_message_register_interested_api:
            if (detail_) {
                detail_->register_interested_apis(ctx);
            } else {
                LOG_TRACE(SERVICE_REMCON, "Player type not set yet to register interested API!");
                ctx->complete(epoc::error_none);
            }
            break;

        default:
            LOG_ERROR(SERVICE_REMCON, "Unimplemented remcon opcode {}", ctx->msg->function);
        }
    }

    void remcon_session::set_player_type(service::ipc_context *ctx) {
        std::optional<epoc::remcon::player_type_information> information = ctx->get_argument_data_from_descriptor<epoc::remcon::player_type_information>(1);

        std::optional<std::string> name = ctx->get_argument_value<std::string>(2);

        if (!information || !name) {
            // On older version of remcon (reversed, it's set client type)
            // First argument is the client type, and that's it
            type_ = static_cast<epoc::remcon::client_type>(*ctx->get_argument_value<std::int32_t>(0));

            switch (type_) {
            case epoc::remcon::client_type_controller:
                detail_ = std::make_unique<epoc::remcon::controller_session>();
                break;

            case epoc::remcon::client_type_target:
                detail_ = std::make_unique<epoc::remcon::target_session>();
                break;

            default:
                break;
            }
        }

        if (information)
            information_ = std::move(information.value());

        if (name)
            name_ = std::move(name.value());
        else
            name_ = "Empty";

        LOG_INFO(SERVICE_REMCON, "Remcon session set player type with name: {}, client type: {},"
                                 " player type: {}, player subtype: {}",
            name_, epoc::remcon::client_type_to_string(type_),
            epoc::remcon::player_type_to_string(information_.type_),
            epoc::remcon::player_subtype_to_string(information_.subtype_));

        ctx->complete(epoc::error_none);
    }
}