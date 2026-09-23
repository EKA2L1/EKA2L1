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

#pragma once

#include <services/framework.h>

#include <cstdint>
#include <memory>
#include <unordered_set>

namespace eka2l1 {
    namespace drivers::hwrm {
        class vibrator;
    }

    // Wire protocol of UIQ 2's appcli.dll (RAppClient and its subsessions).
    enum linnea_opcode {
        linnea_opcode_unregister = 0,
        linnea_opcode_create_plugin_subsession = 2,
        linnea_opcode_close_plugin_subsession = 3,
        linnea_opcode_plugin_request = 4,
        linnea_opcode_plugin_request_2 = 5,
        linnea_opcode_plugin_cancel = 7
    };

    // Plugin id vibratorapi.dll opens its subsession with; vibplugin.asp serves it on hardware.
    static constexpr std::uint32_t LINNEA_VIBRATOR_PLUGIN_ID = 0x1A87;

    class linnea_session : public service::typical_session {
        std::unordered_set<std::int32_t> subsessions_;
        std::int32_t next_subsession_handle_ = 1;

        void create_plugin_subsession(service::ipc_context *ctx);
        void vibrator_request(service::ipc_context *ctx);

    public:
        explicit linnea_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver);
        void fetch(service::ipc_context *ctx) override;
    };

    // UIQ 2 platform application server. On hardware its plugins reach the baseband,
    // so only the vibrator plugin is provided; other plugins are refused.
    class linnea_server : public service::typical_server {
        std::unique_ptr<drivers::hwrm::vibrator> vibrator_;

    public:
        explicit linnea_server(eka2l1::system *sys);
        ~linnea_server() override;

        void connect(service::ipc_context &context) override;

        drivers::hwrm::vibrator *vibrator() {
            return vibrator_.get();
        }
    };
}
