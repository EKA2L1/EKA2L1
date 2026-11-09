// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <control/api.h>
#include <memory>

namespace eka2l1 {
    class system;
}
namespace eka2l1::control {
    constexpr std::uint32_t ngage_games_uid = 0x20003B78;

    class ngage_controller {
        struct impl;
        std::unique_ptr<impl> impl_;

    public:
        ngage_controller();
        ~ngage_controller();
        // Called on the frontend's lifecycle queue; polling never waits for guest execution.
        response request(system &sys, const std::string &command, bool poll);
        void cancel(system &sys);
        void reset(system &sys);
        void clear();
    };
}
