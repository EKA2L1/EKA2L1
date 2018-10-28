/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <atomic>
#include <cstdint>
#include <memory>

#include <debugger/debugger.h>

struct MemoryEditor;

namespace eka2l1 {
    class system;
    struct imgui_logger;

    class imgui_debugger: public debugger_base {
        system *sys;

        bool should_show_threads;
        bool should_show_mutexs;
        bool should_show_chunks;
        
        bool should_pause;
        bool should_stop;
        bool should_load_state;
        bool should_save_state;
        bool should_install_package;
        bool should_show_memory;
        bool should_show_disassembler;
        bool should_show_logger;

        void show_threads();
        void show_mutexs();
        void show_chunks();
        void show_timers();
        void show_disassembler();
        void show_menu();
        void show_memory();

        std::atomic<std::uint64_t> debug_thread_id;

        std::shared_ptr<MemoryEditor> mem_editor;
        std::shared_ptr<imgui_logger> logger;

    public:
        explicit imgui_debugger(eka2l1::system *sys, std::shared_ptr<imgui_logger> logger);

        bool should_emulate_stop() override {
            return should_stop;
        }

        void show_debugger(std::uint32_t width, std::uint32_t height
            , std::uint32_t fb_width, std::uint32_t fb_height) override;
    };
}