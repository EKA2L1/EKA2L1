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

#pragma once

#include <services/sensor/defs.h>
#include <drivers/sensor/sensor.h>

#include <kernel/server.h>
#include <services/framework.h>

#include <map>
#include <memory>

namespace eka2l1 {
    struct sensor_channel {
    protected:
        std::unique_ptr<drivers::sensor> controller_;
        std::size_t new_data_callback_handle_;

    public:
        explicit sensor_channel(std::unique_ptr<drivers::sensor> &controller);
        ~sensor_channel();

        bool listen_for_data(const listening_parameters &parameters);
        void cancel_data_listening();

        std::uint32_t data_packet_size() const {
            return controller_->data_packet_size();
        }

        drivers::sensor *get_sensor_controller() {
            return controller_.get();
        }

        // TODO: Add function for retrieve async data. Once listening is registered, data is regularly
        // requested through another opcode
    };

    class sensor_server : public service::typical_server {
    public:
        explicit sensor_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };

    struct sensor_client_session : public service::typical_session {
    private:        
        std::map<std::uint32_t, std::unique_ptr<sensor_channel>> channels_;
        sensor_channel *get_sensor_channel(const std::uint32_t id);

    public:
        explicit sensor_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void query_channels(eka2l1::service::ipc_context *ctx);
        void open_channel(eka2l1::service::ipc_context *ctx);
        void close_channel(eka2l1::service::ipc_context *ctx);
        void start_listening(eka2l1::service::ipc_context *ctx);
        void stop_listening(eka2l1::service::ipc_context *ctx);
        void get_property(eka2l1::service::ipc_context *ctx);
        void get_all_properties(eka2l1::service::ipc_context *ctx);
    };
}
