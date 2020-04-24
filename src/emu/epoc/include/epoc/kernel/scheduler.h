/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <arm/arm_factory.h>
#include <common/configure.h>
#include <common/queue.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <tuple>
#include <vector>

namespace eka2l1 {
    class timing_system;
    class kernel_system;

    namespace kernel {
        class thread;
        class process;
    }

    namespace manager {
        class script_manager;
    }

    struct thread_comparator {
        bool operator()(const kernel::thread *x, const kernel::thread *y) const;
    };

    namespace kernel {
        enum class thread_state;

        using uid = std::uint32_t;

        class thread_scheduler {
            kernel::thread *readys[64];
            std::uint32_t ready_mask[2]{ 0, 0 };

            kernel::thread *crr_thread;
            kernel::process *crr_process;

            uint32_t ticks_yield;

            arm::arm_interface *jitter;

            int wakeup_evt;
            int yield_evt;

            std::mutex mut;
            timing_system *timing;
            kernel_system *kern;

            manager::script_manager *scripter;

        protected:
            kernel::thread *next_ready_thread();
            void switch_context(kernel::thread *oldt, kernel::thread *newt);

        public:
            // The constructor also register all the needed event
            explicit thread_scheduler(kernel_system *kern, timing_system *sys, manager::script_manager *scripter,
                arm::arm_interface &cpu);

            void queue_thread_ready(kernel::thread *thr);
            void dequeue_thread_from_ready(kernel::thread *thr);

            void reschedule();
            void unschedule_wakeup();
            bool schedule(kernel::thread *thread);
            bool sleep(kernel::thread *thr, uint32_t sl_time, const bool deque = true);
            bool wait(kernel::thread *thr);
            bool resume(kernel::thread *thr);
            void unschedule(kernel::thread *thr);
            bool stop(kernel::thread *thr);

            bool should_terminate() {
                return false;
            }

            kernel::thread *current_thread() const {
                return crr_thread;
            }

            kernel::process *current_process() const {
                return crr_process;
            }
        };
    }
}
