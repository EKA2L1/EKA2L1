/*
 * Copyright (c) 2018 EKA2L1 Team / Citra Emulator Project
 * 
 * This file is part of EKA2L1 project / Citra Emulator Project
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

#pragma once

#include <common/queue.h>
#include <epoc/kernel/thread.h>

namespace eka2l1 {
    namespace kernel {
        /*! \brief A mutex kernel object. 
        */
        class mutex : public kernel_obj {
            //! The lock count
            int lock_count;

            //! Thread holding
            thread_ptr holding;

            cp_queue<thread_ptr> waits;
            cp_queue<thread_ptr> pendings;

            std::vector<thread_ptr> suspended;

            timing_system *timing;

            int mutex_event_type;

        protected:
            void wake_next_thread();

        public:
            mutex(kernel_system *kern, timing_system *timing)
                : kernel_obj(kern), timing(timing) {
                obj_type = kernel::object_type::mutex;
            }

            mutex(kernel_system *kern, timing_system *timing, std::string name, bool init_locked,
                        kernel::access_type access = kernel::access_type::local_access);

            /*! \brief Timeout reached, whether it's on the pendings
            */
            void waking_up_from_suspension(std::uint64_t userdata, int cycles_late);

            void wait();
            void try_wait();

            void wait_for(int msecs);

            bool signal();

            /*! \brief This update the mutex accordingly to the priority.
             *
             * If the thread is in wait_mutex state, and its priority is increased, 
             * it's put to the pending queue.
             *
             * If the thread is in the pending queue, and its priority is smaller
             * then the highest priority thread in the wait queue, it will be put
             * back to the wait queue.
             */
            void priority_change(thread *thr);

            /*! \brief Call when a suspend call is invoked. 
             *
             * If the thread is in wait_mutex, it will be put into a suspend container.
             *
             * If the thread is in hold_mutex_pending, the next waiting thread will be
             * call and put to the pending queue. 
             *
             * Note that pending thread that wait to claim the mutex is still eligable to run. In the state
             * of hold_mutex_pending, the emulator PC will be saved back to the call to the SVC, so the 
             * thread can claim the mutex manually. 
            */
            bool suspend_thread(thread *thr);
            bool unsuspend_thread(thread *thr);

            void do_state(common::chunkyseri &seri) override;
        };
    }
}