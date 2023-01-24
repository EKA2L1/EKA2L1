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

    class window_server;
}

class main_window;

namespace eka2l1::desktop {
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
        bool init_app_launched;
        bool inited_graphics;
        bool stretch_to_fill_display;

        common::event graphics_event;
        common::event init_event;
        common::event pause_event;
        common::event kill_event;

        config::state conf;
        window_server *winserv;

        std::mutex lockdown;
        std::size_t sys_reset_cbh;

        main_window *ui_main;
        int present_status;

        explicit emulator();

        void stage_one();
        bool stage_two();

        void on_system_reset(system *the_sys);
    };
}
