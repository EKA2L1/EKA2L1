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
#include <common/random.h>
#include <common/log.h>

#include <core/core_kernel.h>
#include <core/core_mem.h>
#include <core/kernel/mutex.h>
#include <core/kernel/sema.h>
#include <core/kernel/thread.h>
#include <core/ptr.h>

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
            ctx.cpsr = 0;

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

        thread::thread(kernel_system *kern, memory_system *mem, process_ptr owner,
            kernel::access_type access,
            const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            ptr<void> usrdata,
            thread_priority pri)
            : wait_obj(kern, name, access)
            , own_process(owner)
            , stack_size(stack_size)
            , min_heap_size(min_heap_size)
            , max_heap_size(max_heap_size)
            , usrdata(usrdata)
            , mem(mem) 
            , thread_handles(kern, handle_array_owner::thread) {
            obj_type = object_type::thread;

            priority = caculate_thread_priority(pri);

            /* Here, since reschedule is needed for switching thread and process, primary thread handle are owned by kernel. */

            stack_chunk = kern->create_chunk("", 0, common::align(stack_size, mem->get_page_size()), common::align(stack_size, mem->get_page_size()), prot::read_write,
                chunk_type::normal, chunk_access::local, chunk_attrib::none, owner_type::kernel);

            name_chunk = kern->create_chunk("", 0, common::align(name.length() * 2 + 4, mem->get_page_size()), common::align(name.length() * 2 + 4, mem->get_page_size()), prot::read_write,
                chunk_type::normal, chunk_access::local, chunk_attrib::none, owner_type::kernel);

            request_sema = kern->create_sema("requestSema" + common::to_string(eka2l1::random()), 0, 150, owner_type::kernel);

            sync_msg = kern->create_msg(owner_type::kernel);

            /* Create TDesC string. Combine of string length and name data (USC2) */

            std::u16string name_16(name.begin(), name.end());

            chunk_ptr name_chunk_ptr = std::dynamic_pointer_cast<kernel::chunk>(kern->get_kernel_obj(name_chunk));
            chunk_ptr stack_chunk_ptr = std::dynamic_pointer_cast<kernel::chunk>(kern->get_kernel_obj(stack_chunk));

            memcpy(name_chunk_ptr->base().get(mem), name_16.data(), name.length() * 2);

            const size_t metadata_size = 0x40;

            // Left the space for the program to put thread create information
            const address stack_top = stack_chunk_ptr->base().ptr_address() + stack_size - metadata_size;

            ptr<uint8_t> stack_phys_beg(stack_chunk_ptr->base().ptr_address());
            ptr<uint8_t> stack_phys_end(stack_top);

            uint8_t *start = stack_phys_beg.get(mem);
            uint8_t *end = stack_phys_end.get(mem);

            // Fill the stack with garbage
            std::fill(start, end, 0xcc);
            create_stack_metadata(ptr<void>(stack_top), name.length(), name_chunk_ptr->base().ptr_address(), epa);

            reset_thread_ctx(epa, stack_top);
            scheduler = kern->get_thread_scheduler();
        }
        
        bool thread::should_wait(thread_ptr thr) {
            return state != thread_state::stop;
        }

        void thread::acquire(thread_ptr thr) {
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
                // scheduler->schedule(std::reinterpret_pointer_cast<thread>(kern->get_kernel_obj(obj_id)));
            }
        }

        void thread::wait_for_any_request() {
            sema_ptr sema = 
                std::dynamic_pointer_cast<kernel::semaphore>(kern->get_kernel_obj(request_sema));

            sema->wait();
        }

        void thread::signal_request() {
            sema_ptr sema = std::dynamic_pointer_cast<kernel::semaphore>(kern->get_kernel_obj(request_sema));

            sema->release(1);
        }

        kernel_obj_ptr thread::get_object(uint32_t handle) {
            return thread_handles.get_object(handle);
        }
    }
}
