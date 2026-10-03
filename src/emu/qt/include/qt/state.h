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
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include <common/queue.h>
#include <common/sync.h>
#include <config/app_settings.h>
#include <config/config.h>
#include <package/manager.h>
#include <system/epoc.h>

#include <drivers/audio/audio.h>
#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/graphics.h>
#include <drivers/input/emu_controller.h>
#include <drivers/sensor/sensor.h>

namespace eka2l1 {
    namespace drivers {
        class graphics_driver;
        class audio_driver;
    }

    namespace kernel {
        class process;
    }

    class window_server;

    namespace control {
        class server;
    }
}

class main_window;

namespace eka2l1::desktop {
    class control_frontend;

    /**
     * \brief State of the emulator on desktop.
     */
    struct emulator {
        std::unique_ptr<system> symsys;
        std::unique_ptr<drivers::graphics_driver> graphics_driver;
        std::unique_ptr<drivers::audio_driver> audio_driver;
        std::unique_ptr<drivers::sensor_driver> sensor_driver;
        std::unique_ptr<config::app_settings> app_settings;

        drivers::emu_window *window;
        drivers::emu_controller_ptr joystick_controller;

        std::atomic<bool> should_emu_quit;
        std::atomic<bool> should_emu_pause;
        std::atomic<bool> stage_two_inited;

        bool first_time;
        bool init_fullscreen;
        bool app_launch_from_command_line;
        bool inited_graphics;
        bool stretch_to_fill_display;

        // The graphics thread tells the OS thread its driver is ready with graphics_event; the OS
        // thread tells the graphics thread the system is gone, at shutdown, with
        // system_released_event. One event for both lets the graphics thread swallow its own
        // signal when the emulator shuts down before the OS thread took it.
        common::event graphics_event;
        common::event system_released_event;

        // init_event asks the OS thread to (re)attempt the stage two initialisation, init_done_event
        // reports an attempt back. They must stay separate: outside of Win32 common::event auto-resets
        // on wait, so one event for both directions lets the requester swallow its own signal.
        common::event init_event;
        common::event init_done_event;

        common::event pause_event;
        common::event kill_event;

        // Set once the graphics thread has made its driver. Shutting down waits for it: the main
        // loop can end before then (an exit asked for over the control socket during startup).
        common::event graphics_driver_ready_event;

        config::state conf;
        window_server *winserv;

        std::mutex lockdown;
        std::size_t sys_reset_cbh;

        main_window *ui_main;
        int present_status;

        std::string launched_app_name_;

        // Where --control asked the control server to listen; empty when it was not given.
        std::string control_endpoint;
        std::unique_ptr<control_frontend> control_host;
        std::unique_ptr<control::server> control_server;

        explicit emulator();
        ~emulator();

        void stage_one();
        bool stage_two();

        void on_system_reset(system *the_sys);
    };
}
