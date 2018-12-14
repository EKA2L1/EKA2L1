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

#include <epoc/kernel/process.h>
#include <epoc/kernel/scheduler.h>
#include <epoc/kernel/libmanager.h>

#include <epoc/loader/romimage.h>

namespace eka2l1::kernel {
    void process::create_prim_thread(uint32_t code_addr, uint32_t ep_off, uint32_t stack_size, uint32_t heap_min,
        uint32_t heap_max) {
        page_table *last = mem->get_current_page_table();
        mem->set_current_page_table(page_tab);

        // Reason (bentokun): EKA2 thread trying to access 0x400008, which is hardcoded and invalid. Until we can
        // find out why, just leave it here
        // TODO (bentokun): Find out why and remove this hack
        mem->chunk(0x400000, 0, 0x1000, 0x1000, prot::read_write);

        primary_thread
            = kern->create_thread(kernel::owner_type::kernel,
                nullptr,
                kernel::access_type::local_access,
                process_name + "::Main", ep_off,
                stack_size, heap_min, heap_max,
                true,
                0, 0, kernel::priority_normal);

        ++thread_count;

        if (last) {
            mem->set_current_page_table(*last);
        }

        uint32_t dll_lock_handle = kern->create_mutex("dllLockMutexProcess" + common::to_string(puid),
            false, kernel::owner_type::kernel, kernel::access_type::local_access);

        dll_lock = std::reinterpret_pointer_cast<kernel::mutex>(kern->get_kernel_obj(dll_lock_handle));
    }
    
    process::process(kernel_system *kern, memory_system *mem)
        : kernel_obj(kern), mem(mem), page_tab(mem->get_page_size()) {
        obj_type = kernel::object_type::process;
    }

    process::process(kernel_system *kern, memory_system *mem, uint32_t uid,
        const std::string &process_name, const std::u16string &exe_path,
        const std::u16string &cmd_args, loader::e32img_ptr &img,
        const process_priority pri)
        : kernel_obj(kern, process_name, access_type::local_access)
        , puid(uid)
        , process_name(process_name)
        , kern(kern)
        , mem(mem)
        , romimg(nullptr)
        , img(img)
        , exe_path(exe_path)
        , cmd_args(cmd_args)
        , priority(pri)
        , page_tab(mem->get_page_size())
        , process_handles(kern, handle_array_owner::process) {
        obj_type = kernel::object_type::process;

        // Preserve the page table
        create_prim_thread(img->rt_code_addr, img->rt_code_addr + img->header.entry_point, img->header.stack_size,
            img->header.heap_size_min, img->header.heap_size_max);
    }

    process::process(kernel_system *kern, memory_system *mem, uint32_t uid,
        const std::string &process_name, const std::u16string &exe_path,
        const std::u16string &cmd_args, loader::romimg_ptr &img,
        const process_priority pri)
        : kernel_obj(kern, process_name, access_type::local_access)
        , puid(uid)
        , process_name(process_name)
        , kern(kern)
        , mem(mem)
        , img(nullptr)
        , romimg(img)
        , exe_path(exe_path)
        , cmd_args(cmd_args)
        , page_tab(mem->get_page_size())
        , priority(pri)
        , process_handles(kern, handle_array_owner::process) {
        obj_type = kernel::object_type::process;

        create_prim_thread(
            romimg->header.code_address, romimg->header.entry_point,
            romimg->header.stack_size, romimg->header.heap_minimum_size, romimg->header.heap_maximum_size);
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
        thread_ptr thr = kern->get_thread_by_handle(primary_thread);

        if (!thr) {
            return false;
        }

        kern->run_thread(primary_thread);

        return true;
    }

    std::uint32_t process::get_entry_point_address() {
        return romimg ? romimg->header.entry_point : (img->rt_code_addr + img->header.entry_point);
    }

    void process::set_priority(const process_priority new_pri) {
        std::vector<thread_ptr> all_own_threads = kern->get_all_thread_own_process(kern->get_process(obj_name));
        priority = new_pri;

        for (const auto &thr : all_own_threads) {
            thr->update_priority();
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

    void process::signal_dll_lock() {
        dll_lock->signal();
    }

    security_info process::get_sec_info() {
        security_info info;

        if (img) {
            info.secure_id = img->header_extended.info.secure_id;
            info.vendor_id = img->header_extended.info.vendor_id;

            info.caps[0] = img->header_extended.info.cap1;
            info.caps[1] = img->header_extended.info.cap2;
        } else {
            info.secure_id = romimg->header.sec_info.secure_id;
            info.vendor_id = romimg->header.sec_info.vendor_id;

            info.caps[0] = romimg->header.sec_info.cap1;
            info.caps[1] = romimg->header.sec_info.cap2;
        }

        return info;
    }
    
    void process::logon_request_form::do_state(kernel_system *kern, common::chunkyseri &seri) {
        auto s = seri.section("ProcessLogonRequest", 1);

        if (!s) {
            return;
        }

        std::uint64_t requester_id = (requester ? requester->unique_id() : 0);

        seri.absorb(requester_id);
        seri.absorb(request_status.ptr_address());

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            requester = std::reinterpret_pointer_cast<kernel::thread>(
                kern->get_kernel_obj_by_id(requester_id));
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
        seri.absorb(primary_thread);
        seri.absorb(thread_count);
        seri.absorb(flags);
        seri.absorb(priority);
        seri.absorb(exit_reason);
        seri.absorb(exit_type);
        
        seri.absorb(process_name);
        seri.absorb(exe_path);
        seri.absorb(cmd_args);

        bool xip = (img ? false : true);
        seri.absorb(xip); 

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            hle::lib_manager *libmngr = kern->get_lib_manager();

            if (xip) {
                img = libmngr->load_e32img(common::utf8_to_ucs2(process_name));
                romimg = nullptr;

                // Lib manager only saves a brief information of the cache list but not the loader list.
                // Must open image again, however, it's XIP since the code memory region is preserved.
                libmngr->open_e32img(img);
            } else {
                img = nullptr;
                romimg = libmngr->load_romimg(common::utf8_to_ucs2(process_name));

                libmngr->open_romimg(romimg);
            }
        }

        for (std::uint8_t i = 0; i < 15; i++) {
            args[i].do_state(seri);
        }

        std::uint32_t cs = static_cast<std::uint32_t>(logon_requests.size());
        seri.absorb(cs);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            logon_requests.resize(cs);
        }

        for (auto &lr: logon_requests) {
            lr.do_state(kern, seri);
        }

        cs = static_cast<std::uint32_t>(rendezvous_requests.size());
        seri.absorb(cs);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            rendezvous_requests.resize(s);
        }

        for (auto &rr: rendezvous_requests) {
            rr.do_state(kern, seri);
        }

        std::uint64_t dll_lock_uid = (dll_lock ? dll_lock->unique_id() : 0);
        seri.absorb(dll_lock_uid);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            dll_lock = std::reinterpret_pointer_cast<kernel::mutex>(
                kern->get_kernel_obj_by_id(dll_lock_uid)
            );
        }
    }
}
