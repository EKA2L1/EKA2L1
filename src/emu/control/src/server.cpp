/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include "context.h"

#include <control/dispatcher.h>
#include <control/endpoint.h>
#include <control/rpc_server.h>
#include <control/server.h>

#include <cstdlib>
#include <cstring>

namespace eka2l1::control {
    static constexpr const char *TOKEN_VARIABLE = "EKA2L1_CONTROL_TOKEN";

    // Anyone on the machine can reach a loopback port; a short token could be guessed.
    static constexpr std::size_t MIN_TOKEN_LENGTH = 16;

    class server_impl {
    public:
        dispatcher rpc;
        rpc_server transport;
        context ctx;

        explicit server_impl(frontend &host)
            : transport(rpc)
            , ctx(host) {
            add_emulator_methods(rpc, ctx);
            add_app_methods(rpc, ctx);
        }
    };

    server::server(frontend &host)
        : impl_(std::make_unique<server_impl>(host)) {
    }

    server::~server() {
        stop();
    }

    bool server::start(const std::string &endpoint_text, std::string &error) {
        const std::optional<endpoint> where = endpoint::parse(endpoint_text, error);

        if (!where) {
            return false;
        }

        if (where->kind == endpoint::tcp) {
            const char *token = std::getenv(TOKEN_VARIABLE);

            if (!token || !*token) {
                error = std::string("A TCP control endpoint needs a token: set ") + TOKEN_VARIABLE
                    + " for the emulator, and have clients call auth with it";
                return false;
            }

            if (std::strlen(token) < MIN_TOKEN_LENGTH) {
                error = std::string(TOKEN_VARIABLE) + " must be at least " + std::to_string(MIN_TOKEN_LENGTH)
                    + " bytes long; a random one is best (openssl rand -hex 16)";
                return false;
            }

            impl_->rpc.require_token(token);
        }

        impl_->ctx.stopping = false;
        return impl_->transport.start(*where, error);
    }

    void server::stop() {
        // Handlers waiting on the guest give up first, so the server thread can finish.
        impl_->ctx.stopping = true;
        impl_->transport.stop();
    }

    dispatcher &server::get_dispatcher() {
        return impl_->rpc;
    }
}
