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
    struct dsp_manager;

    enum dsp_medium_type {
        DSP_MEDIUM_TYPE_EPOC_STREAM = 0,
        DSP_MEDIUM_TYPE_EPOC_PLAYER = 1
    };

    struct dsp_medium {
    protected:
        dsp_manager *manager_;
        std::uint32_t logical_volume_;

        dsp_medium_type type_;

    public:
        explicit dsp_medium(dsp_manager *manager, dsp_medium_type type);
        virtual ~dsp_medium() {}

        virtual std::uint32_t volume() const {
            return logical_volume_;
        }

        dsp_medium_type type() const {
            return type_;
        }

        virtual std::uint32_t max_volume() const = 0;
        virtual void volume(const std::uint32_t volume) = 0;
    };

    struct dsp_epoc_stream : public dsp_medium {
        std::unique_ptr<drivers::dsp_stream> ll_stream_;
        epoc::notify_info copied_info_;

        std::mutex lock_;

        explicit dsp_epoc_stream(std::unique_ptr<drivers::dsp_stream> &stream, dsp_manager *manager);
        ~dsp_epoc_stream() override;

        void volume(const std::uint32_t volume) override;
        std::uint32_t max_volume() const override;

        std::uint32_t volume() const override {
            return dsp_medium::volume();
        }
    };

    enum dsp_epoc_player_flags {
        dsp_epoc_player_flags_prepare_play_when_queue = 1 << 0
    };

    struct dsp_epoc_player : public dsp_medium {
    public:
        std::unique_ptr<epoc::rw_des_stream> custom_stream_;
        std::unique_ptr<drivers::player> impl_;

        std::uint32_t flags_;

        explicit dsp_epoc_player(std::unique_ptr<drivers::player> &impl, dsp_manager *manager, const std::uint32_t init_flags);
        ~dsp_epoc_player() override;

        bool should_prepare_play_when_queue() const {
            return flags_ & dsp_epoc_player_flags_prepare_play_when_queue;
        }

        void volume(const std::uint32_t volume) override;
        std::uint32_t max_volume() const override;

        std::uint32_t volume() const override {
            return dsp_medium::volume();
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

    struct dsp_manager {
    private:
        std::atomic<std::uint32_t> master_volume_;
        std::unique_ptr<dsp_epoc_audren_sema> audren_sema_;
        object_manager<dsp_medium> mediums_;

    public:
        explicit dsp_manager();

        object_manager<dsp_medium>::handle add_object(std::unique_ptr<dsp_medium> &obj) {
            return mediums_.add_object(obj);
        }

        template <typename T>
        T *get_object(const object_manager<dsp_medium>::handle handle) {
            dsp_medium *medium = mediums_.get_object(handle);
            if (!medium) {
                return nullptr;
            }

            if constexpr(std::is_same_v<T, dsp_epoc_stream>) {
                if (medium->type() != DSP_MEDIUM_TYPE_EPOC_STREAM) {
                    return nullptr;
                }
            }

            if constexpr(std::is_same_v<T, dsp_epoc_player>) {
                if (medium->type() != DSP_MEDIUM_TYPE_EPOC_PLAYER) {
                    return nullptr;
                }
            }

            return reinterpret_cast<T*>(medium);
        }

        bool remove_object(const object_manager<dsp_medium>::handle handle) {
            return mediums_.remove_object(handle);
        }

        dsp_epoc_audren_sema *audio_renderer_semaphore() {
            return audren_sema_.get();
        }

        void master_volume(const std::uint32_t volume);
        std::uint32_t master_volume() const;
    };

    struct dispatcher {
    private:
        kernel::chunk *trampoline_chunk_;
        hle::lib_manager *libmngr_;
        memory_system *mem_;

        std::uint32_t trampoline_allocated_;
        dsp_manager dsp_manager_;

        void shutdown();

    public:
        window_server *winserv_;

        ntimer *timing_;

        explicit dispatcher(kernel_system *kern, ntimer *timing);
        ~dispatcher();

        bool patch_libraries(const std::u16string &path, patch_info *patches,
            const std::size_t patch_count);

        void resolve(eka2l1::system *sys, const std::uint32_t function_ord);
        void update_all_screens(eka2l1::system *sys);

        dsp_manager &get_dsp_manager() {
            return dsp_manager_;
        }
    };
}
