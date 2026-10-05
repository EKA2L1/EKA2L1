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

namespace eka2l1 {
    class system;

    namespace kernel {
        class process;
    }
}

namespace eka2l1::control {
    /**
     * \brief What the control server needs from the frontend that embeds it.
     *
     * The emulation itself is reached through system; this covers what each frontend does
     * its own way: pausing its emulation thread, quitting, and keeping its UI in step.
     * Unless noted, methods are called on the server's thread.
     */
    class frontend {
    public:
        virtual ~frontend() = default;

        /**
         * \brief The emulated system, or null while there is none.
         */
        virtual system *get_system() = 0;

        /**
         * \brief Stop running guest code until resume(). Takes effect at the next slice.
         */
        virtual void pause() = 0;
        virtual void resume() = 0;
        virtual bool is_paused() = 0;

        /**
         * \brief Ask the frontend to shut the emulator down. Returns at once.
         *
         * The frontend stops the control server as part of its shutdown, which still delivers
         * the response to the request that asked for the exit.
         *
         * \param exit_code The exit code of the emulator process.
         */
        virtual void request_exit(const int exit_code) = 0;

        /**
         * \brief Return once the work the frontend queued so far has been done.
         *
         * A frontend that reacts to an app exiting later, on a thread of its own (the Qt
         * frontend reboots the device and shows its app list), finishes that here, so a launch
         * right after an exit event starts in the rebooted system. Return early when `cancel`
         * becomes true.
         */
        virtual void settle(const std::atomic<bool> &cancel) {
        }

        /**
         * \brief Called after app.launch started an app, with the emulation locked out: on the
         * emulation thread, or on the control server's thread while the emulation thread is
         * outside system::loop().
         */
        virtual void on_app_launched(kernel::process *app) {
        }

        /**
         * \brief The exit callback the frontend attaches to apps it launches itself, so apps
         * launched over the control protocol are treated the same. May be empty.
         */
        virtual std::function<void(kernel::process *)> app_exit_callback() {
            return nullptr;
        }

        /**
         * \brief Called after a package was installed or removed and the app list rescanned.
         */
        virtual void on_packages_changed() {
        }
    };
}
