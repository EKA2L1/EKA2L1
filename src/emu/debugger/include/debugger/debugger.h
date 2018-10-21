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

#include <cstdint>

namespace eka2l1 {
    class system;

    class debugger {
        system *sys;

        bool should_show_threads;
        bool should_show_mutexs;
        bool should_show_chunks;

        void show_threads();
        void show_mutexs();
        void show_chunks();
        void show_timers();
        void show_disassembler();
        void show_menu();

    public:
        explicit debugger(eka2l1::system *sys)
            : sys(sys) {}

        void show_debugger(std::uint32_t width, std::uint32_t height
            , std::uint32_t fb_width, std::uint32_t fb_height);
    };
}