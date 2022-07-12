/*
 * Copyright (c) 2020 EKA2L1 Team
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

#pragma once

#include <services/bluetooth/btmidman.h>

#include <kernel/server.h>
#include <services/framework.h>
#include <memory>

namespace eka2l1 {
    std::string get_btman_server_name_by_epocver(const epocver ver);

    enum btman_opcode {
        btman_security_open = 1,
        btman_baseband_security_open = 2,
        btman_security_close = 3,
        btman_security_register_service = 6,
        btman_security_new_link_state = 11
    };

    class btman_server : public service::typical_server {
    private:
        std::unique_ptr<epoc::bt::midman> mid_;

    public:
        explicit btman_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
        void device_name(const std::u16string &new_name);

        bool is_oldarch();

        epoc::bt::midman *get_midman() {
            return mid_.get();
        }
    };

    struct btman_client_session : public service::typical_session {
        explicit btman_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);
        void fetch(service::ipc_context *ctx) override;
    };
}
