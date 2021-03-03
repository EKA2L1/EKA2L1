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

#include <utils/des.h>
#include <utils/reqsts.h>

#include <atomic>
#include <vector>
#include <memory>

// Foward declarations
namespace eka2l1 {
    class window_server;
    class kernel_system;
    class memory_system;
    class system;
    class ntimer;

    namespace kernel {
        class chunk;
    }

    namespace hle {
        class lib_manager;
    }
}

namespace eka2l1::dispatch {
    struct patch_info;
    struct dsp_epoc_audren_sema;

    struct dsp_epoc_stream {
        std::unique_ptr<drivers::dsp_stream> ll_stream_;
        epoc::notify_info copied_info_;

        dsp_epoc_audren_sema *audren_sema_;

        std::mutex lock_;

        explicit dsp_epoc_stream(std::unique_ptr<drivers::dsp_stream> &stream, dsp_epoc_audren_sema *sema);
        ~dsp_epoc_stream();
    };

    enum dsp_epoc_player_flags {
        dsp_epoc_player_flags_prepare_play_when_queue = 1 << 0
    };

    struct dsp_epoc_player {
    public:
        std::unique_ptr<epoc::rw_des_stream> custom_stream_;
        std::unique_ptr<drivers::player> impl_;
        std::uint32_t flags_;

        dsp_epoc_audren_sema *audren_sema_;

        explicit dsp_epoc_player(std::unique_ptr<drivers::player> &impl, dsp_epoc_audren_sema *sema, const std::uint32_t init_flags)
            : impl_(std::move(impl))
            , flags_(init_flags)
            , audren_sema_(sema) {
        }

        bool should_prepare_play_when_queue() const {
            return flags_ & dsp_epoc_player_flags_prepare_play_when_queue;
        }
    };

    struct dsp_epoc_audren_sema {
        std::atomic<std::int64_t> own_;

    public:
        explicit dsp_epoc_audren_sema();

        bool free() const;

        // Argument reserved
        void acquire(void * /* owner */);
        void release(void * /* owner */);
    };

    struct dispatcher {
    private:
        std::unique_ptr<dsp_epoc_audren_sema> audren_sema_;

        kernel::chunk *trampoline_chunk_;

        hle::lib_manager *libmngr_;
        memory_system *mem_;

        std::uint32_t trampoline_allocated_;

        void shutdown();

    public:
        window_server *winserv_;

        object_manager<dsp_epoc_player> audio_players_;
        object_manager<dsp_epoc_stream> dsp_streams_;

        ntimer *timing_;

        explicit dispatcher(kernel_system *kern, ntimer *timing);
        ~dispatcher();

        bool patch_libraries(const std::u16string &path, patch_info *patches,
            const std::size_t patch_count);

        dsp_epoc_audren_sema *get_audren_sema();

        void resolve(eka2l1::system *sys, const std::uint32_t function_ord);
        void update_all_screens(eka2l1::system *sys);
    };
}