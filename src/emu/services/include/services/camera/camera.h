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

#include <common/vecx.h>

#include <cstdint>
#include <memory>

namespace eka2l1 {
    namespace drivers::camera {
        class instance;
    }

    class camera_session : public service::typical_session {
        struct capture_state;
        struct feed_state;

        bool low_quality_;
        bool night_mode_;
        std::unique_ptr<drivers::camera::instance> camera_;
        std::shared_ptr<capture_state> pending_capture_;
        std::shared_ptr<feed_state> feed_;
        eka2l1::vec2 feed_size_;
        std::uint32_t feed_format_;

        int turn_on(system *sys);
        void turn_off();

        // Both run with the kernel lock held, and start_feed() drops it around the
        // driver call because a failing driver invokes the callback synchronously.
        bool start_feed(system *sys, const eka2l1::vec2 &size, const std::uint32_t format,
            const std::shared_ptr<capture_state> &first_request);
        void stop_feed();

        static void deliver_frame(const std::shared_ptr<capture_state> &state, const void *buffer,
            const std::size_t buffer_size, const int error);

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
