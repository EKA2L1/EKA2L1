/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/framework.h>
#include <epoc/services/akn/icon/common.h>

namespace eka2l1 {
    class fbs_server;

    class akn_icon_server_session: public service::typical_session {
    public:
        explicit akn_icon_server_session(service::typical_server *svr, service::uid client_ss_uid);
        void fetch(service::ipc_context *ctx) override;
    };

    class akn_icon_server: public service::typical_server {
        enum flags {
            akn_icon_srv_flag_inited = 0x1
        };
        
        epoc::akn_icon_init_data init_data;
        std::uint32_t flags {0};

        fbs_server *fbss;

        void init_server();

    public:
        epoc::akn_icon_init_data *get_init_data() {
            return &init_data;
        }

        explicit akn_icon_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };
}
