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

#include <cstdint>
#include <drivers/audio/dsp.h>
#include <drivers/audio/player.h>
#include <dispatch/management.h>

#include <utils/reqsts.h>

#include <vector>

// Foward declarations
namespace eka2l1 {
    class window_server;
    class kernel_system;
    class system;
    class ntimer;
}

namespace eka2l1::dispatch {
    struct audio_event {
        epoc::notify_info info_;
        std::uint64_t start_ticks_;
        std::uint64_t start_host_;
        audio_event *next_;

        enum {
            FLAG_COMPLETED = 1 << 0
        };

        std::uint32_t flags_;

        explicit audio_event();
    };

    struct dsp_epoc_stream {
        std::unique_ptr<drivers::dsp_stream> ll_stream_;
        audio_event evt_queue_;

        std::mutex lock_;

        explicit dsp_epoc_stream(std::unique_ptr<drivers::dsp_stream> &stream);
        ~dsp_epoc_stream();

        audio_event *get_event(const eka2l1::ptr<epoc::request_status> req_sts);
        void delete_event(audio_event *evt);

        void deliver_audio_events(kernel_system *kern, ntimer *timing);
    };

    struct dispatcher {
    private:
        void shutdown();

    public:
        window_server *winserv_;

        object_manager<drivers::player> audio_players_;
        object_manager<dsp_epoc_stream> dsp_streams_;

        int audio_nof_complete_evt_;
        ntimer *timing_;

        explicit dispatcher();
        ~dispatcher();

        void init(kernel_system *kern, ntimer *timing);

        void resolve(eka2l1::system *sys, const std::uint32_t function_ord);
        void update_all_screens(eka2l1::system *sys);
    };
}