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
#include <common/log.h>
#include <core_kernel.h>
#include <core_mem.h>
#include <kernel/thread.h>
#include <ptr.h>

namespace eka2l1 {
    namespace kernel {
        int caculate_thread_priority(thread_priority pri) {
            const uint8_t pris[] = {
                1, 1, 2, 3, 4, 5, 22, 0,
                3, 5, 6, 7, 8, 9, 22, 0,
                3, 10, 11, 12, 13, 14, 22, 0,
                3, 17, 18, 19, 20, 22, 23, 0,
                9, 15, 16, 21, 24, 25, 28, 0,
                9, 15, 16, 21, 24, 25, 28, 0,
                9, 15, 16, 21, 24, 25, 28, 0,
                18, 26, 27, 28, 29, 30, 31, 0
            };

            // The owning process, in this case is always have the priority
            // of 3 (foreground)

            int idx = (3 << 3) + (int)pri;
            return pris[idx];
        }

        void thread::reset_thread_ctx(uint32_t entry_point, uint32_t stack_top) {
            ctx.pc = entry_point;
            ctx.sp = stack_top;
            ctx.cpsr = 16 | ((entry_point & 1) << 5);

            std::fill(ctx.cpu_registers.begin(), ctx.cpu_registers.end(), 0);
            std::fill(ctx.fpu_registers.begin(), ctx.fpu_registers.end(), 0);
        }

        thread::thread(kernel_system *kern, memory_system *mem, uint32_t owner, const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            void *usrdata,
            thread_priority pri)
            : wait_obj(kern, name)
            , stack_size(stack_size)
            , min_heap_size(min_heap_size)
            , max_heap_size(max_heap_size)
            , usrdata(usrdata)
            , mem(mem)
            , owner(owner) {
            priority = caculate_thread_priority(pri);

            stack_chunk = kern->create_chunk("", 0, stack_size, stack_size, prot::read_write,
                chunk_type::normal, chunk_access::local, chunk_attrib::none);

            const address stack_top = stack_chunk->base().ptr_address() + stack_size;

            ptr<uint8_t> stack_phys_beg(stack_chunk->base().ptr_address());
            ptr<uint8_t> stack_phys_end(stack_chunk->base().ptr_address() + stack_size);

            // Fill the stack with garbage
            std::fill(stack_phys_beg.get(mem), stack_phys_end.get(mem), 0xcc);

            reset_thread_ctx(epa, stack_top);
            scheduler = kern->get_thread_scheduler();
        }

        bool thread::sleep(int64_t ns) {
            state = thread_state::wait;
            return scheduler->sleep(obj_id, ns);
        }

        bool thread::run() {
            state = thread_state::run;
            kern->run_thread(obj_id);

            return true;
        }

        bool thread::stop() {
            scheduler->unschedule_wakeup();

            if (state == thread_state::ready) {
                scheduler->unschedule(obj_id);
            }

            state = thread_state::stop;

            wake_up_waiting_threads();

            for (auto &thr : waits) {
                thr->erase_waiting_thread(thr->unique_id());
            }

            waits.clear();

            // release mutex

            return true;
        }

        bool thread::resume() {
            return scheduler->resume(obj_id);
        }

        bool thread::should_wait(const kernel::uid id) {
            return state != thread_state::stop;
        }

        void thread::acquire(const kernel::uid id) {
            // :)
        }
    }
}

