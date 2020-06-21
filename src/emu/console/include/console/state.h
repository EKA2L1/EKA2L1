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

#include <common/queue.h>
#include <common/sync.h>
#include <epoc/epoc.h>
#include <config/config.h>

#include <debugger/renderer/renderer.h>
#include <drivers/graphics/emu_window.h>
#include <drivers/input/emu_controller.h>

#include <imgui.h>

namespace eka2l1 {
    struct imgui_logger;
    class imgui_debugger;

    namespace drivers {
        class graphics_driver;
        class audio_driver;
    }

    class window_server;
}

namespace eka2l1::desktop {
    /**
     * \brief State of the emulator on desktop.
     */
    struct emulator {
        std::unique_ptr<system> symsys;
        std::unique_ptr<std::thread> graphics_driver_thread;
        std::unique_ptr<drivers::graphics_driver> graphics_driver;
        std::unique_ptr<drivers::audio_driver> audio_driver;
        std::unique_ptr<debugger_renderer> deb_renderer;
        std::unique_ptr<imgui_debugger> debugger;
        std::shared_ptr<imgui_logger> logger;
        drivers::emu_window_ptr window;
        drivers::emu_controller_ptr joystick_controller;

        std::atomic<bool> should_emu_quit;
        std::atomic<bool> should_emu_pause;
        std::atomic<bool> should_ui_quit;
        std::atomic<bool> stage_two_inited;

        eka2l1::request_queue<std::u16string> launch_requests;

        bool first_time;

        common::semaphore graphics_sema;

        config::state conf;
        window_server *winserv;

        bool mouse_down[5];
        std::mutex input_mutex;

        void stage_one();
        void stage_two();
    };
}