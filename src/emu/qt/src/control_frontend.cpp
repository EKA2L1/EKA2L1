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

#include <qt/control_frontend.h>
#include <qt/mainwindow.h>
#include <qt/state.h>

#include <QCoreApplication>
#include <QMetaObject>

#include <chrono>
#include <memory>
#include <thread>

namespace eka2l1::desktop {
    control_frontend::control_frontend(emulator &state)
        : state_(state) {
    }

    system *control_frontend::get_system() {
        return state_.symsys.get();
    }

    // The window exists from before the server starts until after it stops, but runs on the
    // UI thread: reach it through queued calls, once the main loop runs.
    void control_frontend::run_on_ui_thread(std::function<void()> work) {
        const std::lock_guard<std::mutex> guard(ui_mut_);

        if (!ui_running_) {
            ui_backlog_.push_back(std::move(work));
            return;
        }

        QMetaObject::invokeMethod(QCoreApplication::instance(), std::move(work), Qt::QueuedConnection);
    }

    void control_frontend::main_loop_starting() {
        const std::lock_guard<std::mutex> guard(ui_mut_);
        ui_running_ = true;

        for (std::function<void()> &work : ui_backlog_) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), std::move(work), Qt::QueuedConnection);
        }

        ui_backlog_.clear();
    }

    void control_frontend::pause() {
        // The same switch the Pause action flips; the OS thread parks after its slice.
        state_.should_emu_pause = true;

        run_on_ui_thread([this]() {
            if (state_.ui_main) {
                state_.ui_main->show_paused(true);
            }
        });
    }

    void control_frontend::resume() {
        state_.should_emu_pause = false;
        state_.pause_event.set();

        run_on_ui_thread([this]() {
            if (state_.ui_main) {
                state_.ui_main->show_paused(false);
            }
        });
    }

    bool control_frontend::is_paused() {
        return state_.should_emu_pause;
    }

    void control_frontend::request_exit(const int exit_code) {
        // Leave the main loop as closing the window does, but keep the window: the graphics
        // thread may still be setting up on it, and the shutdown destroys it after that thread.
        run_on_ui_thread([exit_code]() {
            QCoreApplication::exit(exit_code);
        });
    }

    void control_frontend::settle(const std::atomic<bool> &cancel) {
        // Calls queued to the UI thread run in order, so once this one ran, so did an
        // app-exit handler queued before it (and the device reboot it does).
        auto reached = std::make_shared<std::atomic<bool>>(false);

        run_on_ui_thread([reached]() {
            *reached = true;
        });

        while (!*reached && !cancel) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    void control_frontend::on_app_launched(kernel::process *app) {
        // Show the display instead of the app list, as when the user starts an app.
        run_on_ui_thread([this]() {
            if (state_.ui_main) {
                state_.ui_main->setup_and_switch_to_game_mode();
            }
        });
    }

    std::function<void(kernel::process *)> control_frontend::app_exit_callback() {
        // Back to the app list and a rebooted device when the app exits, as for the UI's launches.
        return state_.ui_main ? state_.ui_main->get_process_exit_callback() : nullptr;
    }

    void control_frontend::on_packages_changed() {
        // The server rescanned the registrations already, between emulation slices; only the
        // list on screen is behind.
        run_on_ui_thread([this]() {
            if (state_.ui_main) {
                state_.ui_main->reload_applist();
            }
        });
    }
}
