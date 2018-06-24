/*
 * Copyright (c) 2018 EKA2L1 Team / Citra Team
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

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <core_kernel.h>
#include <core_mem.h>
#include <kernel/mutex.h>
#include <kernel/sema.h>
#include <kernel/thread.h>
#include <ptr.h>

namespace eka2l1 {
    namespace kernel {
        struct epoc9_thread_create_info {
            int handle;
            int type;
            address func_ptr;
            address ptr;
            address supervisor_stack;
            int supervisor_stack_size;
            address user_stack;
            int user_stack_size;
            int init_thread_priority;
            uint32_t name_len;
            address name_ptr;
            int total_size;
        };

        struct epoc9_std_epoc_thread_create_info : public epoc9_thread_create_info {
            address allocator;
            int heap_min;
            int heap_max;
            int padding;
        };

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

        void thread::create_stack_metadata(ptr<void> stack_ptr, uint32_t name_len, address name_ptr, const address epa) {
            epoc9_std_epoc_thread_create_info info;

            // This is intended to make EPOC HLE side create RHeap
            info.allocator = 0;
            info.func_ptr = epa;
            info.ptr = usrdata.ptr_address();

            // The handle to RThread. HLE function ignore this, however when a RThread HLE call is executed, this
            // will point to that RThread Handle
            info.handle = 0;
            info.heap_min = min_heap_size;
            info.heap_max = max_heap_size;
            info.init_thread_priority = priority;

            info.name_len = name_len;
            info.name_ptr = name_ptr;
            info.supervisor_stack = 0;
            info.supervisor_stack_size = 0;

            info.user_stack = stack_ptr.ptr_address();
            info.user_stack_size = stack_size;
            info.padding = 0;

            info.total_size = 0x40;

            memcpy(stack_ptr.get(mem), &info, 0x40);
        }

        thread::thread(kernel_system *kern, memory_system *mem, kernel::owner_type owner, kernel::uid owner_id, kernel::access_type access,
            const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            ptr<void> usrdata,
            thread_priority pri)
            : wait_obj(kern, name, owner, owner_id, access)
            , stack_size(stack_size)
            , min_heap_size(min_heap_size)
            , max_heap_size(max_heap_size)
            , usrdata(usrdata)
            , mem(mem) {
            obj_type = object_type::thread;

            priority = caculate_thread_priority(pri);

            stack_chunk = kern->create_chunk("stackThreadID" + common::to_string(obj_id), 0, stack_size, stack_size, prot::read_write,
                chunk_type::normal, chunk_access::local, chunk_attrib::none, owner_type::thread, obj_id);

            name_chunk = kern->create_chunk("", 0, common::align(name.length() * 2 + 4, mem->get_page_size()), common::align(name.length() * 2 + 4, mem->get_page_size()), prot::read_write,
                chunk_type::normal, chunk_access::local, chunk_attrib::none, owner_type::thread, obj_id);

            //tls_chunk = kern->create_chunk("", 0, common::align(50 * 12, mem->get_page_size()), common::align(name.length() * 2 + 4, mem->get_page_size()), prot::read_write,
            //    chunk_type::normal, chunk_access::local, chunk_attrib::none, owner_type::thread, obj_id);

            request_sema = kern->create_sema("requestSemaFor" + common::to_string(obj_id), 0, 150, owner_type::thread);

            sync_msg = kern->create_msg(owner_type::process);

            /* Create TDesC string. Combine of string length and name data (USC2) */

            std::u16string name_16(name.begin(), name.end());

            memcpy(name_chunk->base().get(mem), name_16.data(), name.length() * 2);

            const size_t metadata_size = 0x40;

            // Left the space for the program to put thread create information
            const address stack_top = stack_chunk->base().ptr_address() + stack_size - metadata_size;

            ptr<uint8_t> stack_phys_beg(stack_chunk->base().ptr_address());
            ptr<uint8_t> stack_phys_end(stack_chunk->base().ptr_address() + stack_size);

            // Fill the stack with garbage
            std::fill(stack_phys_beg.get(mem), stack_phys_end.get(mem), 0xcc);
            create_stack_metadata(ptr<void>(stack_top), name.length(), name_chunk->base().ptr_address(), epa);

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

            if (waits.size() > 0)
                for (auto &thr : waits) {
                    thr->erase_waiting_thread(thr->unique_id());
                }

            waits.clear();

            scheduler->reschedule();

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

        tls_slot *thread::get_tls_slot(uint32_t handle, uint32_t dll_uid) {
            for (uint32_t i = 0; i < ldata.tls_slots.size(); i++) {
                if (ldata.tls_slots[i].handle != -1 && ldata.tls_slots[i].handle == handle) {
                    return &ldata.tls_slots[i];
                }
            } 

            for (uint32_t i = 0; i < ldata.tls_slots.size(); i++) {
                if (ldata.tls_slots[i].handle == -1) {
                    ldata.tls_slots[i].handle = handle;
                    ldata.tls_slots[i].uid = dll_uid;

                    return &ldata.tls_slots[i];
                }
            }

            return nullptr;
        }

        void thread::close_tls_slot(tls_slot &slot) {
            slot.handle = -1;
        }

        void thread::update_priority() {
            int new_priority = current_priority();

            for (auto &mut : held_mutexes) {
                if (mut->get_priority() < new_priority) {
                    new_priority = mut->get_priority();
                }
            }

            priority = new_priority;

            if (state == kernel::thread_state::ready) {
                scheduler->refresh();
            } else {
                scheduler->schedule(std::reinterpret_pointer_cast<thread>(kern->get_kernel_obj(obj_id)));
            }
        }

        void thread::wait_for_any_request() {
            request_sema->acquire(obj_id);

            if (request_sema->should_wait(obj_id)) {
                request_sema->add_waiting_thread(std::reinterpret_pointer_cast<kernel::thread>(kern->get_kernel_obj(obj_id)));

                state = thread_state::wait_fast_sema;

                scheduler->reschedule();
            }
        }

        void thread::signal_request() {
            request_sema->release(1);
        }
    }
}
