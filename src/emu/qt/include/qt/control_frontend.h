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

#include <control/frontend.h>

#include <functional>
#include <mutex>
#include <vector>

namespace eka2l1::desktop {
    struct emulator;

    /**
     * \brief Connects the control server (--control) to the desktop emulator and its window.
     */
    class control_frontend : public control::frontend {
        emulator &state_;

        // Work for the window waits here until the main loop runs: before that the window is
        // still being built (it spins events while it loads the app list), and an exit asked for
        // then would be forgotten by exec().
        std::mutex ui_mut_;
        bool ui_running_ = false;
        std::vector<std::function<void()>> ui_backlog_;

        void run_on_ui_thread(std::function<void()> work);

    public:
        explicit control_frontend(emulator &state);

        /**
         * \brief Call right before the main loop starts: work queued for the window runs from now on.
         */
        void main_loop_starting();

        system *get_system() override;

        void pause() override;
        void resume() override;
        bool is_paused() override;

        void request_exit(const int exit_code) override;
        void settle(const std::atomic<bool> &cancel) override;

        void on_app_launched(kernel::process *app) override;
        std::function<void(kernel::process *)> app_exit_callback() override;
        void on_packages_changed() override;
    };
}
