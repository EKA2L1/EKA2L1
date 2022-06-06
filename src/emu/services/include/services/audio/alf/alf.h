/*
 * Copyright (c) 2022 EKA2L1 Team
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

#include <services/framework.h>

namespace eka2l1 {
    class alf_streamer_session : public service::typical_session {
    public:
        explicit alf_streamer_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version);
        void fetch(service::ipc_context *ctx) override;
    };

    class alf_streamer_server : public service::typical_server {
    public:
        explicit alf_streamer_server(system *sys);
        void connect(service::ipc_context &context) override;
    };
}