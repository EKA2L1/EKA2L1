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
#include <common/random.h>

#include <core/core_kernel.h>
#include <core/core_mem.h>
#include <core/kernel/mutex.h>
#include <core/kernel/sema.h>
#include <core/kernel/thread.h>
#include <core/ptr.h>

#include <common/e32inc.h>
#include <e32err.h>

int after_timout_evt = -1;

static void after_thread_timeout(std::uint64_t data, int cycles_late) {
    eka2l1::kernel::thread *thr = reinterpret_cast<decltype(thr)>(data);

    if (thr == nullptr) {
        return;
    }

    thr->notify_after(0);
}

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

        int map_thread_priority_to_calc(thread_priority pri) {
            switch (pri) {
            case thread_priority::priority_much_less:
                return -7;

            case thread_priority::priority_less:
                return -6;

            case thread_priority::priority_normal:
                return -5;

            case thread_priority::priority_more:
                return -4;

            case thread_priority::priority_much_more:
                return -3;

            case thread_priority::priority_real_time:
                return -2;

            case thread_priority::priority_null:
                return 0;

            case thread_priority::priority_absolute_very_low:
                return 1;

            case thread_priority::priority_absolute_low:
                return 5;

            case thread_priority::priority_absolute_background:
                return 10;

            case thread_priority::priorty_absolute_foreground:
                return 15;

            case thread_priority::priority_absolute_high:
                return 23;

            default: {
                LOG_WARN("Undefined priority.");
                break;
            }
            }

            return 1;
        }

        int map_process_pri_calc(process_priority pri) {
            switch (pri) {
            case process_priority::low:
                return 0;
            case process_priority::background:
                return 1;
            case process_priority::foreground:
                return 2;
            case process_priority::high:
                return 3;
            case process_priority::window_svr:
                return 4;
            case process_priority::file_svr:
                return 5;
            case process_priority::supervisor:
                return 6;
            case process_priority::real_time_svr:
                return 7;
            default: {
                LOG_WARN("Undefined process priority");
                break;
            }
            }

            return 0;
        }

        int caculate_thread_priority(process_ptr pr, thread_priority pri) {
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

            int tp = map_thread_priority_to_calc(pri);

            if (tp >= 0) {
                return (tp < 64) ? tp : 63;
            }

            int prinew = tp + 8;

            if (prinew < 0)
                prinew = 0;

            int idx = (map_process_pri_calc(pr->get_priority()) << 3) + static_cast<int>(prinew);
            return pris[idx];
        }

        void thread::reset_thread_ctx(uint32_t entry_point, uint32_t stack_top, bool should_create_heap) {
            std::fill(ctx.cpu_registers.begin(), ctx.cpu_registers.end(), 0);
            std::fill(ctx.fpu_registers.begin(), ctx.fpu_registers.end(), 0);

            ctx.pc = entry_point;

            if (should_create_heap) {
                ctx.pc = kern->get_lib_manager()->get_export_addr(4238211793); // Setup thread heap
                ctx.lr = entry_point;

                ctx.cpu_registers[1] = stack_top;
                ctx.cpu_registers[0] = 0;
            }

            ctx.sp = stack_top;
            ctx.cpsr = ((ctx.pc & 1) << 5);
        }

        void thread::create_stack_metadata(ptr<void> stack_ptr, ptr<void> allocator, uint32_t name_len,
            address name_ptr, const address epa) {
            epoc9_std_epoc_thread_create_info info;
            info.allocator = allocator.ptr_address();
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

        thread::thread(kernel_system *kern, memory_system *mem, timing_system *timing, process_ptr owner,
            kernel::access_type access,
            const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            bool inital,
            ptr<void> usrdata,
            ptr<void> allocator,
            thread_priority pri)
            : kernel_obj(kern, name, access)
            , own_process(owner)
            , stack_size(stack_size)
            , min_heap_size(min_heap_size)
            , max_heap_size(max_heap_size)
            , usrdata(usrdata)
            , mem(mem)
            , priority(pri)
            , timing(timing)
            , timeout_sts(nullptr)
            , wait_obj(nullptr)
            , sleep_nof_sts(nullptr)
            , thread_handles(kern, handle_array_owner::thread) {
            if (after_timout_evt == -1) {
                after_timout_evt = timing->register_event("ThreadAfterTimoutEvt", &after_thread_timeout);
            }

            if (owner) {
                owner->increase_thread_count();
                real_priority = caculate_thread_priority(own_process, pri);
                last_priority = real_priority;
            }

            create_time = timing->get_ticks();

            obj_type = object_type::thread;
            state = thread_state::create; // Suspended.

            /* Here, since reschedule is needed for switching thread and process, primary thread handle are owned by kernel. */

            stack_chunk = kern->create_chunk("", 0, common::align(stack_size, mem->get_page_size()), common::align(stack_size, mem->get_page_size()), prot::read_write,
                chunk_type::normal, chunk_access::local, chunk_attrib::none, owner_type::kernel);

            name_chunk = kern->create_chunk("", 0, common::align(name.length() * 2 + 4, mem->get_page_size()), common::align(name.length() * 2 + 4, mem->get_page_size()), prot::read_write,
                chunk_type::normal, chunk_access::local, chunk_attrib::none, owner_type::kernel);

            request_sema = std::dynamic_pointer_cast<kernel::semaphore>(
                kern->get_kernel_obj(kern->create_sema("requestSema" + common::to_string(eka2l1::random()), 0, owner_type::kernel)));

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
            create_stack_metadata(ptr<void>(stack_top), allocator, name.length(), name_chunk_ptr->base().ptr_address(), epa);

            bool should_create_heap = (!allocator.ptr_address()) && (!inital);

            reset_thread_ctx(epa, stack_top, should_create_heap);
            scheduler = kern->get_thread_scheduler();

            if (allocator.ptr_address()) {
                ldata.heap = allocator;
            }
        }

        thread::~thread() {
            own_process->decrease_thread_count();
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

        void thread::after(epoc::request_status *sts, uint32_t mssecs) {
            assert(!timeout_sts && "After request outstanding");
            timeout_sts = sts;

            timing->schedule_event(timing->us_to_cycles((uint64_t)mssecs), 
                after_timout_evt, reinterpret_cast<uint64_t>(this));
        }

        bool thread::sleep(uint32_t mssecs) {
            return scheduler->sleep(std::dynamic_pointer_cast<kernel::thread>(
                kern->get_kernel_obj_by_id(uid)), mssecs);
        }

        bool thread::sleep_nof(epoc::request_status *sts, uint32_t mssecs) {
            assert(!sleep_nof_sts && "Thread supposed to sleep already");
            sleep_nof_sts = sts;

            return scheduler->sleep(std::dynamic_pointer_cast<kernel::thread>(
                kern->get_kernel_obj_by_id(uid)), mssecs);
        }

        void thread::notify_sleep(const int errcode) {
            if (sleep_nof_sts) {
                sleep_nof_sts->status = errcode;
                sleep_nof_sts = 0;
            }

            sleep_nof_sts = nullptr;
        }

        void thread::notify_after(const int errcode) {
            if (timeout_sts) {
                timeout_sts->status = errcode;
                timeout_sts = 0;

                signal_request();
            }

            timeout_sts = nullptr;
        }

        bool thread::stop() {
            return scheduler->stop(std::dynamic_pointer_cast<kernel::thread>(kern->get_kernel_obj_by_id(uid)));
        }

        void thread::update_priority() {
            last_priority = real_priority;
            real_priority = caculate_thread_priority(own_process, priority);

            int new_priority = current_real_priority();
            real_priority = new_priority;

            if (wait_obj) {
                switch (wait_obj->get_object_type()) {
                case object_type::mutex: {
                    reinterpret_cast<mutex *>(wait_obj)->priority_change(this);
                    break;
                }

                case object_type::sema: {
                    reinterpret_cast<semaphore *>(wait_obj)->priority_change();
                    break;
                }

                default:
                    break;
                }
            }

            if (state == kernel::thread_state::ready || state == kernel::thread_state::hold_mutex_pending) {
                scheduler->refresh();
            }
        }

        void thread::set_priority(const thread_priority new_pri) {
            priority = new_pri;

            if (state == kernel::thread_state::ready) {
                scheduler->refresh();
            }

            update_priority();
            kern->prepare_reschedule();
        }

        void thread::wait_for_any_request() {
            request_sema->wait();
        }

        void thread::signal_request(int count) {
            request_sema->signal(count);
        }

        bool thread::suspend() {
            bool res = scheduler->wait(std::dynamic_pointer_cast<kernel::thread>(kern->get_kernel_obj_by_id(uid)));

            if (!res) {
                return false;
            }

            res = false;
            state = thread_state::wait;

            // Call wait object to handle suspend event
            if (!wait_obj) {
                switch (wait_obj->get_object_type()) {
                case object_type::mutex: {
                    res = reinterpret_cast<mutex *>(wait_obj)->suspend_thread(this);
                    break;
                }

                case object_type::sema: {
                    res = reinterpret_cast<semaphore *>(wait_obj)->suspend_waiting_thread(this);
                    break;
                }

                default:
                    break;
                }
            }

            return res;
        }

        bool thread::resume() {
            bool res = scheduler->resume(std::dynamic_pointer_cast<kernel::thread>(
                kern->get_kernel_obj_by_id(uid)));

            if (!res) {
                return false;
            }

            res = false;
            state = thread_state::ready;

            // Call wait object to handle suspend event
            if (!wait_obj) {
                switch (wait_obj->get_object_type()) {
                case object_type::mutex: {
                    res = reinterpret_cast<mutex *>(wait_obj)->unsuspend_thread(this);
                    break;
                }

                case object_type::sema: {
                    res = reinterpret_cast<semaphore *>(wait_obj)->unsuspend_waiting_thread(this);
                    break;
                }

                default:
                    break;
                }
            }

            return res;
        }

        void thread::owning_process(process_ptr pr) {
            own_process = pr;
            own_process->increase_thread_count();

            chunk_ptr name_chunk_ptr = std::dynamic_pointer_cast<kernel::chunk>(kern->get_kernel_obj(name_chunk));
            chunk_ptr stack_chunk_ptr = std::dynamic_pointer_cast<kernel::chunk>(kern->get_kernel_obj(stack_chunk));

            name_chunk_ptr->set_own_process(own_process);
            stack_chunk_ptr->set_own_process(own_process);

            update_priority();
            last_priority = real_priority;
        }

        kernel_obj_ptr thread::get_object(uint32_t handle) {
            return thread_handles.get_object(handle);
        }

        bool thread::operator>(const thread &rhs) {
            if (real_priority == rhs.real_priority) {
                if (uid < rhs.uid) {
                    return true;
                }

                return false;
            }

            return real_priority > rhs.real_priority;
        }

        bool thread::operator<(const thread &rhs) {
            if (real_priority == rhs.real_priority) {
                // First thread comes first
                if (uid > rhs.uid) {
                    return true;
                }

                return false;
            }

            return real_priority < rhs.real_priority;
        }

        bool thread::operator==(const thread &rhs) {
            return (real_priority == rhs.real_priority) && (uid <= rhs.uid);
        }

        bool thread::operator>=(const thread &rhs) {
            if (real_priority == rhs.real_priority) {
                if (uid <= rhs.uid) {
                    return true;
                }

                return false;
            }

            return real_priority >= rhs.real_priority;
        }

        bool thread::operator<=(const thread &rhs) {
            if (real_priority == rhs.real_priority) {
                if (uid >= rhs.uid) {
                    return true;
                }

                return false;
            }

            return real_priority <= rhs.real_priority;
        }

        void thread::logon(epoc::request_status *logon_request, bool rendezvous) {
            if (state == thread_state::stop) {
                *logon_request = exit_reason;
                return;
            }

            if (rendezvous) {
                rendezvous_requests.push_back(logon_request_form{ kern->crr_thread(), logon_request });
                return;
            }

            logon_requests.push_back(logon_request_form{ kern->crr_thread(), logon_request });
        }

        bool thread::logon_cancel(epoc::request_status *logon_request, bool rendezvous) {
            if (rendezvous) {
                auto req_info = std::find_if(rendezvous_requests.begin(), rendezvous_requests.end(),
                    [&](logon_request_form &form) { return form.request_status == logon_request; });

                if (req_info != rendezvous_requests.end()) {
                    *logon_request = -3;
                    rendezvous_requests.erase(req_info);

                    return true;
                }

                return false;
            }

            auto req_info = std::find_if(logon_requests.begin(), logon_requests.end(),
                [&](logon_request_form &form) { return form.request_status == logon_request; });

            if (req_info != logon_requests.end()) {
                *logon_request = -3;
                logon_requests.erase(req_info);

                return true;
            }

            return false;
        }

        void thread::rendezvous(int rendezvous_reason) {
            for (auto &ren : rendezvous_requests) {
                *(ren.request_status) = rendezvous_reason;
                ren.requester->signal_request();
            }

            rendezvous_requests.clear();
        }

        void thread::finish_logons() {
            for (auto &req : logon_requests) {
                *(req.request_status) = exit_reason;
                req.requester->signal_request();
            }

            for (auto &req : rendezvous_requests) {
                *(req.request_status) = exit_reason;
                req.requester->signal_request();
            }

            logon_requests.clear();
            rendezvous_requests.clear();
        }
    }
}
