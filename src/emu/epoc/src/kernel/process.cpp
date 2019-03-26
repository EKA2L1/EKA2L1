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

#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>

#include <epoc/kernel.h>
#include <epoc/mem.h>

#include <epoc/kernel/codeseg.h>
#include <epoc/kernel/libmanager.h>
#include <epoc/kernel/process.h>
#include <epoc/kernel/scheduler.h>

namespace eka2l1::kernel {
    void process::create_prim_thread(uint32_t code_addr, uint32_t ep_off, uint32_t stack_size, uint32_t heap_min,
        uint32_t heap_max, kernel::thread_priority pri) {
        page_table *last = mem->get_current_page_table();
        mem->set_current_page_table(page_tab);

        primary_thread
            = kern->create<kernel::thread>(
                mem,
                kern->get_timing_system(),
                nullptr,
                kernel::access_type::local_access,
                process_name + "::Main", ep_off,
                stack_size, heap_min, heap_max,
                true,
                0, 0, pri);

        ++thread_count;

        if (last) {
            mem->set_current_page_table(*last);
        }

        dll_lock = kern->create<kernel::mutex>(kern->get_timing_system(),
            "dllLockMutexProcess" + common::to_string(puid),
            false, kernel::access_type::local_access);
    }

    process::process(kernel_system *kern, memory_system *mem)
        : kernel_obj(kern)
        , mem(mem)
        , page_tab(mem->get_page_size()) {
        obj_type = kernel::object_type::process;
    }

    process::process(kernel_system *kern, memory_system *mem, uint32_t uid,
        const std::string &process_name, const std::u16string &exe_path,
        const std::u16string &cmd_args, codeseg_ptr arg_codeseg,
        uint32_t stack_size, uint32_t heap_min, uint32_t heap_max,
        const process_priority pri)
        : kernel_obj(kern, process_name, access_type::local_access)
        , puid(uid)
        , process_name(process_name)
        , kern(kern)
        , mem(mem)
        , exe_path(exe_path)
        , cmd_args(cmd_args)
        , page_tab(mem->get_page_size())
        , priority(pri)
        , codeseg(std::move(arg_codeseg))
        , process_handles(kern, handle_array_owner::process) {
        obj_type = kernel::object_type::process;
        sec_info = codeseg->get_sec_info();

        if (!process_name.empty() && process_name.back() == '\0') {
            this->process_name.pop_back();
        }

        if (kern->get_epoc_version() >= epocver::eka2) {
            std::string all_caps;

            // Log all capabilities for debugging
            for (int i = 0; i < epoc::cap_limit; i++) {
                if (sec_info.caps.get(i)) {
                    all_caps += epoc::capability_to_string(static_cast<epoc::capability>(i));
                    all_caps += " ";
                }
            }

            LOG_INFO("Process {} capabilities: {}", process_name, all_caps.empty() ? "None" : all_caps);
        }

        create_prim_thread(
            codeseg->get_code_run_addr(), codeseg->get_entry_point(),
            stack_size, heap_min, heap_max,
            kernel::thread_priority::priority_absolute_foreground_normal);

        // TODO: Load all references DLL in the export list.
    }

    void process::set_arg_slot(std::uint8_t slot, std::uint8_t *data, std::size_t data_size) {
        if (slot >= 16 || args[slot].used) {
            return;
        }

        args[slot].data.resize(data_size);
        args[slot].used = true;

        std::copy(data, data + data_size, args[slot].data.begin());
    }

    std::optional<pass_arg> process::get_arg_slot(uint8_t slot) {
        if (slot >= 16) {
            return std::optional<pass_arg>{};
        }

        return args[slot];
    }

    process_uid_type process::get_uid_type() {
        return std::tuple(0x1000007A, 0x100039CE, puid);
    }

    kernel_obj_ptr process::get_object(uint32_t handle) {
        return process_handles.get_object(handle);
    }

    bool process::run() {
        return kern->get_thread_scheduler()->schedule(primary_thread);
    }

    std::uint32_t process::get_entry_point_address() {
        return codeseg->get_entry_point();
    }

    void process::set_priority(const process_priority new_pri) {
        priority = new_pri;

        for (const auto &thr : kern->threads) {
            // I think this will be invoked not much, but this sure hurts performance
            if (thr->owning_process()->unique_id() == uid) {
                thr->update_priority();
            }
        }
    }

    void *process::get_ptr_on_addr_space(address addr) {
        if (!page_tab.pointers[addr / page_tab.page_size]) {
            return nullptr;
        }

        return static_cast<void *>(page_tab.pointers[addr / page_tab.page_size]
            + addr % page_tab.page_size);
    }

    // EKA2L1 doesn't use multicore yet, so rendezvous and logon
    // are just simple.
    void process::logon(eka2l1::ptr<epoc::request_status> logon_request, bool rendezvous) {
        if (!thread_count) {
            *(logon_request.get(kern->crr_process())) = exit_reason;
            return;
        }

        if (rendezvous) {
            rendezvous_requests.push_back(logon_request_form{ kern->crr_thread(), logon_request });
            return;
        }

        logon_requests.push_back(logon_request_form{ kern->crr_thread(), logon_request });
    }

    bool process::logon_cancel(eka2l1::ptr<epoc::request_status> logon_request, bool rendezvous) {
        if (rendezvous) {
            auto req_info = std::find_if(rendezvous_requests.begin(), rendezvous_requests.end(),
                [&](logon_request_form &form) { return form.request_status.ptr_address() == logon_request.ptr_address(); });

            if (req_info != rendezvous_requests.end()) {
                *(logon_request.get(req_info->requester->owning_process())) = -3;
                rendezvous_requests.erase(req_info);

                return true;
            }

            return false;
        }

        auto req_info = std::find_if(logon_requests.begin(), logon_requests.end(),
            [&](logon_request_form &form) { return form.request_status.ptr_address() == logon_request.ptr_address(); });

        if (req_info != logon_requests.end()) {
            *(logon_request.get(req_info->requester->owning_process())) = -3;
            logon_requests.erase(req_info);

            return true;
        }

        return false;
    }

    void process::rendezvous(int rendezvous_reason) {
        exit_reason = rendezvous_reason;
        exit_type = process_exit_type::pending;

        for (auto &ren : rendezvous_requests) {
            *(ren.request_status.get(ren.requester->owning_process())) = rendezvous_reason;

            LOG_TRACE("Rendezvous to: {}", ren.requester->name());

            ren.requester->signal_request();
        }

        rendezvous_requests.clear();
    }

    void process::finish_logons() {
        for (auto &req : logon_requests) {
            *(req.request_status.get(req.requester->owning_process())) = exit_reason;
            req.requester->signal_request();
        }

        for (auto &req : rendezvous_requests) {
            *(req.request_status.get(req.requester->owning_process())) = exit_reason;
            req.requester->signal_request();
        }

        logon_requests.clear();
        rendezvous_requests.clear();
    }

    void process::wait_dll_lock() {
        dll_lock->wait();
    }

    void process::signal_dll_lock(thread_ptr callee) {
        dll_lock->signal(callee);
    }

    epoc::security_info process::get_sec_info() {
        return sec_info;
    }

    bool process::satisfy(epoc::security_policy &policy, epoc::security_info *missing) {
        // Do not enforce security on EKA1. It's not even there
        if (kern->get_epoc_version() >= epocver::eka2) {
            return true;
        }

        [[maybe_unused]] epoc::security_info missing_holder;
        return policy.check(sec_info, missing ? *missing : missing_holder);
    }

    bool process::has(epoc::capability_set &cap_set) {
        // Do not enforce security on EKA1. It's not even there
        if (kern->get_epoc_version() >= epocver::eka2) {
            return true;
        }

        return sec_info.has(cap_set);
    }

    void process::get_memory_info(memory_info &info) {
        info.rt_code_addr = codeseg->get_code_run_addr();
        info.rt_const_data_addr = codeseg->get_data_run_addr();
        info.rt_bss_addr = info.rt_const_data_addr;
        info.rt_bss_size = codeseg->get_bss_size();
        // TODO: More
    }

    void process::logon_request_form::do_state(kernel_system *kern, common::chunkyseri &seri) {
        auto s = seri.section("ProcessLogonRequest", 1);

        if (!s) {
            return;
        }

        std::uint32_t requester_id = (requester ? requester->unique_id() : 0);

        seri.absorb(requester_id);
        seri.absorb(request_status.ptr_address());

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            requester = kern->get<kernel::thread>(requester_id);
        }
    }

    void pass_arg::do_state(common::chunkyseri &seri) {
        auto s = seri.section("PassArg", 1);

        if (!s) {
            return;
        }

        seri.absorb(used);
        seri.absorb_container(data);
    }

    void process::do_state(common::chunkyseri &seri) {
        auto s = seri.section("Process", 1);

        if (!s) {
            return;
        }

        seri.absorb(puid);

        kernel::uid prim_thread_id = primary_thread->unique_id();
        seri.absorb(prim_thread_id);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            primary_thread = kern->get<kernel::thread>(prim_thread_id);
        }

        seri.absorb(thread_count);
        seri.absorb(flags);
        seri.absorb(priority);
        seri.absorb(exit_reason);
        seri.absorb(exit_type);

        seri.absorb(process_name);
        seri.absorb(exe_path);
        seri.absorb(cmd_args);

        // We don't need to do state for page table, eventaully it will be filled in by chunk
        // Do state for object table
        process_handles.do_state(seri);
    }
}
