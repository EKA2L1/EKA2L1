/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <drivers/audio/player.h>
#include <epoc/dispatch/management.h>
#include <cstdint>

// Foward declarations
namespace eka2l1 {
    class window_server;
    class kernel_system;
    class system;
}

namespace eka2l1::dispatch {
    struct dispatcher {
    public:
        window_server *winserv_;
        object_manager<drivers::player> audio_players_;

        explicit dispatcher();
        void init(kernel_system *kern);

        void resolve(eka2l1::system *sys, const std::uint32_t function_ord);
    };
}