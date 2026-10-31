// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <control/api.h>

namespace eka2l1::control {
    // Frontends execute atomic operations on their own control/UI queues.
    // Timing, sequences, waits and long-operation jobs are shared by all transports.
    class service {
        struct impl;
        std::shared_ptr<impl> impl_;

    public:
        explicit service(handler frontend);
        ~service();
        service(const service &) = delete;
        service &operator=(const service &) = delete;
        void handle(request req, completion done);
        void stop();
    };
}
