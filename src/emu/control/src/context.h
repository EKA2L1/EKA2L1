/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace eka2l1 {
    class system;
    class kernel_system;
    class applist_server;
}

namespace eka2l1::control {
    class dispatcher;
    class frontend;

    /**
     * \brief What the method handlers share.
     */
    struct context {
        frontend &host;

        // Set when the server stops; a handler waiting on the guest gives up.
        std::atomic<bool> stopping{ false };

        explicit context(frontend &host);

        /**
         * \brief The emulated system, or throws rpc_error(error_not_ready).
         */
        system &require_system();

        /**
         * \brief Run `task` between emulation slices (see system::post_task()) and wait for it.
         *
         * Rethrows what the task throws. Throws rpc_error(error_shutting_down) if the server
         * stops before the task started.
         */
        void run_in_guest(const std::function<void(system &)> &task);
    };

    // Lookups that need a booted device; they throw rpc_error(error_not_ready) without one.
    // Call them inside run_in_guest(): the services are rebuilt when the system resets.
    kernel_system &require_kernel(system &sys);
    applist_server &require_applist(system &sys);

    void add_emulator_methods(dispatcher &rpc, context &ctx);
    void add_app_methods(dispatcher &rpc, context &ctx);
}
