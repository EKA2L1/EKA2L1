// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <control/api.h>
#include <functional>
#include <memory>

namespace eka2l1 {
    class system;
    namespace kernel {
        class process;
    }
}
namespace eka2l1::control {
    enum class native_change { devices,
        applications,
        configuration };

    struct native_frontend {
        std::string platform;
        std::string documents;
        std::string log;
        std::function<system *()> get_system;
        std::function<std::uint64_t()> frames;
        std::function<bool()> can_capture = [] { return true; };
        std::function<bool(std::uint32_t)> launch;
        std::function<void()> close;
        std::function<bool(int)> boot;
        std::function<void(bool)> pause;
        std::function<void(native_change)> changed;
        std::function<void(const double *)> acceleration;
    };

    class native_backend {
        struct impl;
        std::shared_ptr<impl> impl_;

    public:
        explicit native_backend(native_frontend frontend);
        ~native_backend();
        // The frontend serializes these calls with its normal lifecycle operations.
        response handle(const request &req);
        bool launch_app(std::uint32_t uid, std::function<void(kernel::process *)> exited);
        void reset_session();
    };
}
