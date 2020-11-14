/*
 * Copyright (c) 2019 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project
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
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include <common/log.h>
#include <common/queue.h>
#include <common/sync.h>
#include <system/epoc.h>
#include <package/manager.h>
#include <config/app_settings.h>
#include <config/config.h>

#include <drivers/graphics/emu_window.h>
#include <drivers/input/emu_controller.h>
#include <android/emu_window_android.h>
#include <android/launcher.h>
#include <services/window/window.h>

namespace eka2l1 {
    namespace drivers {
        class graphics_driver;
        class audio_driver;
    }

    class launcher;
}

namespace eka2l1::android {
    /**
     * \brief State of the emulator on desktop.
     */
    struct emulator {
        std::unique_ptr<system> symsys;
        std::unique_ptr<drivers::graphics_driver> graphics_driver;
        std::unique_ptr<drivers::audio_driver> audio_driver;
        std::unique_ptr<launcher> launcher;

        std::shared_ptr<base_logger> logger;
        std::unique_ptr<config::app_settings> app_settings;
        std::unique_ptr<drivers::emu_window_android> window;
        drivers::emu_controller_ptr joystick_controller;

        std::atomic<bool> should_emu_quit;
        std::atomic<bool> should_emu_pause;
        std::atomic<bool> should_graphics_pause;
        std::atomic<bool> surface_inited;
        std::atomic<bool> should_ui_quit;
        std::atomic<bool> stage_two_inited;

        bool first_time;

        common::semaphore graphics_sema;
        common::semaphore pause_sema;
        common::semaphore pause_graphics_sema;

        config::state conf;
        window_server *winserv;

        std::mutex input_mutex;

        explicit emulator();

        void stage_one();
        bool stage_two();
    };
}