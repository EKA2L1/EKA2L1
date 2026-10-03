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

#include "context.h"

#include <control/dispatcher.h>
#include <control/frontend.h>

#include <kernel/kernel.h>
#include <services/applist/applist.h>
#include <system/epoc.h>

#include <chrono>
#include <condition_variable>
#include <exception>

namespace eka2l1::control {
    context::context(frontend &host)
        : host(host) {
    }

    system &context::require_system() {
        system *sys = host.get_system();

        if (!sys) {
            throw rpc_error(error_not_ready, "The emulator has no system running");
        }

        return *sys;
    }

    void context::run_in_guest(const std::function<void(system &)> &task) {
        system &sys = require_system();

        enum class task_state {
            pending,
            running,
            done,
            cancelled
        };

        struct shared_state {
            std::mutex mut;
            std::condition_variable done_cond;
            task_state state = task_state::pending;
            std::exception_ptr failure;
        };

        auto shared = std::make_shared<shared_state>();

        sys.post_task([shared, &task, &sys]() {
            {
                const std::lock_guard<std::mutex> guard(shared->mut);

                // The waiter gave up before the task started; `task` may be gone already.
                if (shared->state == task_state::cancelled) {
                    return;
                }

                shared->state = task_state::running;
            }

            try {
                task(sys);
            } catch (...) {
                shared->failure = std::current_exception();
            }

            {
                const std::lock_guard<std::mutex> guard(shared->mut);
                shared->state = task_state::done;
            }

            shared->done_cond.notify_all();
        });

        std::unique_lock<std::mutex> lock(shared->mut);

        while (shared->state != task_state::done) {
            if ((shared->state == task_state::pending) && stopping) {
                shared->state = task_state::cancelled;
                throw rpc_error(error_shutting_down, "The emulator is shutting down");
            }

            // The emulation thread may be outside loop(): paused by the frontend, no device
            // booted yet, or simply between two loop() calls. Then run the queue here, under the
            // same system lock loop() takes.
            lock.unlock();
            sys.try_run_pending_tasks();
            lock.lock();

            if (shared->state != task_state::done) {
                shared->done_cond.wait_for(lock, std::chrono::milliseconds(10));
            }
        }

        lock.unlock();

        // The task may have queued follow-up work, such as an exit event after a kill. Run it
        // now if the emulation thread is parked, instead of when the emulation resumes.
        sys.try_run_pending_tasks();

        if (shared->failure) {
            std::rethrow_exception(shared->failure);
        }
    }

    kernel_system &require_kernel(system &sys) {
        kernel_system *kern = sys.get_kernel_system();

        if (!kern) {
            throw rpc_error(error_not_ready, "No device has been booted");
        }

        return *kern;
    }

    applist_server &require_applist(system &sys) {
        kernel_system &kern = require_kernel(sys);
        applist_server *server = reinterpret_cast<applist_server *>(kern.get_by_name<service::server>(
            get_app_list_server_name_by_epocver(kern.get_epoc_version())));

        if (!server) {
            throw rpc_error(error_not_ready, "No device has been booted");
        }

        return *server;
    }
}
