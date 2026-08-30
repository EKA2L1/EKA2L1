/*
 * Copyright (c) 2026 EKA2L1 Team
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <services/framework.h>

#include <memory>

namespace eka2l1 {
    namespace drivers::camera {
        class instance;
    }

    class camera_session : public service::typical_session {
        struct capture_state;

        bool low_quality_;
        bool night_mode_;
        std::unique_ptr<drivers::camera::instance> camera_;
        std::shared_ptr<capture_state> pending_capture_;

        int turn_on(system *sys);
        void turn_off();

    public:
        camera_session(service::typical_server *server, kernel::uid client_session_uid,
            epoc::version client_version);
        ~camera_session() override;

        void fetch(service::ipc_context *ctx) override;
    };

    class camera_server : public service::typical_server {
    public:
        explicit camera_server(system *sys);

        void connect(service::ipc_context &ctx) override;
    };
}
