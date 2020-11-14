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
#include <config/app_settings.h>

#include <kernel/kernel.h>
#include <mem/mem.h>

#include <kernel/codeseg.h>
#include <kernel/libmanager.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>

#include <mem/mmu.h>
#include <mem/process.h>

namespace eka2l1::kernel {
    std::int32_t process::refresh_generation() {
        if (flags & FLAG_KERNEL_PROCESS) {
            return 0;
        }

        const epoc::uid this_uid = std::get<2>(uids);
        std::int32_t max_generation = 0;

        for (auto &pr_raw: kern->processes_) {
            kernel::process *pr = reinterpret_cast<kernel::process*>(pr_raw.get());
            if ((this_uid == std::get<2>(pr->uids)) && pr->access_count > 0) {
                // If they have same name, increase the generation
                if (common::compare_ignore_case(obj_name.c_str(), pr->obj_name.c_str()) == 0)
                    max_generation = common::max<std::int32_t>(pr->generation_, max_generation);
            }
        }

        return max_generation + 1;
    }

    void process::create_prim_thread(uint32_t code_addr, uint32_t ep_off, uint32_t stack_size, uint32_t heap_min,
        uint32_t heap_max, kernel::thread_priority pri) {
        primary_thread
            = kern->create<kernel::thread>(
                mem,
                kern->get_ntimer(),
                this,
                kernel::access_type::local_access,
                "Main", ep_off,
                stack_size, heap_min, heap_max,
                true,
                0, 0, pri);

        ++thread_count;

        dll_lock = kern->create<kernel::mutex>(kern->get_ntimer(), "dllLockMutexProcess" + common::to_string(std::get<2>(uids)),
            false, kernel::access_type::local_access);
    }

    void process::construct_with_codeseg(codeseg_ptr arg_codeseg, uint32_t stack_size, uint32_t heap_min, uint32_t heap_max,
        const process_priority pri) {
        if (codeseg) {
            return;
        }

        codeseg = std::move(arg_codeseg);

        // Attach this codeseg to our process
        codeseg->attach(this);
    
        // Get security info
        sec_info = codeseg->get_sec_info();

        uids = codeseg->get_uids();
        priority = pri;

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

        exe_path = codeseg->get_full_path();

        // Base on: sprocess.cpp#L245 in kernelhwsrv package
        create_prim_thread(codeseg->get_code_run_addr(this), codeseg->get_entry_point(this), stack_size, heap_min, heap_max,
            kernel::thread_priority::priority_normal);

        reload_compat_setting();

        // TODO: Load all references DLL in the export list.
    }

    static constexpr mem::vm_address get_rom_bss_addr(const mem::mem_model_type type, const bool mem_map_old) {
        switch (type) {
        case mem::mem_model_type::moving:
        case mem::mem_model_type::multiple:
            return (mem_map_old ? (mem::rom_bss_eka1 + mem::ROM_BSS_START_OFFSET_EKA1) :
                (mem::dll_static_data + mem::ROM_BSS_START_OFFSET));

        case mem::mem_model_type::flexible:
            return mem::dll_static_data_flexible + mem::ROM_BSS_START_OFFSET;

        default:
            break;
        }

        return 0;
    }

    static bool is_process_uid_type_change_callback_elem_free(const process_uid_type_change_callback_elem &elem) {
        return !elem.first;
    }

    static void make_process_uid_type_change_callback_elem_free(process_uid_type_change_callback_elem &elem) {
        elem.first = nullptr;
    }

    process::process(kernel_system *kern, memory_system *mem, const std::string &process_name, const std::u16string &exe_path,
        const std::u16string &cmd_args)
        : kernel_obj(kern, process_name, nullptr, access_type::local_access)
        , process_name(process_name)
        , kern(kern)
        , mem(mem)
        , exe_path(exe_path)
        , cmd_args(cmd_args)
        , codeseg(nullptr)
        , process_handles(kern, handle_array_owner::process)
        , flags(0)
        , priority(kernel::process_priority::foreground)
        , exit_type(kernel::entity_exit_type::pending)
        , parent_process_(nullptr)
        , time_delay_(0) 
        , setting_inheritence_(true)
        , generation_(0)
        , uid_change_callbacks(is_process_uid_type_change_callback_elem_free, make_process_uid_type_change_callback_elem_free) {
        obj_type = kernel::object_type::process;

        if (!process_name.empty() && process_name.back() == '\0') {
            this->process_name.pop_back();
        }

        // Create mem model implementation
        mm_impl_ = mem::make_new_mem_model_process(mem->get_mmu(), mem->get_model_type());
        generation_ = refresh_generation();
    }

    void process::destroy() {
        detatch_from_parent();
    }

    std::string process::name() const {
        std::string org = fmt::format("{}[{:08x}]", obj_name, std::get<2>(uids));
        if (!is_kernel_process()) {
            org += fmt::format("{:04x}", generation_ & 0xFFFF);
        }

        return org;
    }

    void process::rename(const std::string &new_name) {
        kernel_obj::rename(new_name);
        generation_ = refresh_generation();
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
        return uids;
    }

    void process::set_uid_type(const process_uid_type &type) {
        uids = std::move(type);
        generation_ = refresh_generation();
        
        reload_compat_setting();

        for (auto &uid_change_callback: uid_change_callbacks) {
            uid_change_callback.second(uid_change_callback.first, uids);
        }
    }

    std::size_t process::register_uid_type_change_callback(void *userdata, process_uid_type_change_callback callback) {
        /** 
         * 我要蟹汤米线。我俄。
         * 先生，我要蟹汤米线。
         * 中国人在看，我要蟹汤米线。 谢谢。
         */
        auto the_pair = std::make_pair(userdata, callback);
        return uid_change_callbacks.add(the_pair);
    }

    bool process::unregister_uid_type_change_callback(const std::size_t handle) {
        // Tôi đói thật. Tôi muốn ăn bún riêu. Nhưng bh đang là tối...
        return uid_change_callbacks.remove(handle);
    }

    kernel_obj_ptr process::get_object(uint32_t handle) {
        return process_handles.get_object(handle);
    }

    bool process::run() {
        return kern->get_thread_scheduler()->schedule(&(*primary_thread));
    }

    std::uint32_t process::get_entry_point_address() {
        return codeseg->get_entry_point(this);
    }

    void process::set_priority(const process_priority new_pri) {
        priority = new_pri;
        
        common::double_linked_queue_element *elem = thread_list.first();
        common::double_linked_queue_element *end = thread_list.end();

        do {
            if (!elem) {
                return;
            }

            E_LOFF(elem, kernel::thread, process_thread_link)->update_priority();
            elem = elem->next;
        } while (elem != end);
    }

    void *process::get_ptr_on_addr_space(address addr) {
        return mem->get_mmu()->get_host_pointer(mm_impl_->address_space_id(), addr);
    }

    // EKA2L1 doesn't use multicore yet, so rendezvous and logon
    // are just simple.
    void process::logon(eka2l1::ptr<epoc::request_status> logon_request, bool rendezvous) {
        if (!thread_count) {
            (logon_request.get(kern->crr_process()))->set(exit_reason, kern->is_eka1());
            return;
        }

        if (rendezvous) {
            rendezvous_requests.emplace_back(logon_request, kern->crr_thread());
            return;
        }

        logon_requests.emplace_back(logon_request, kern->crr_thread());
    }

    bool process::logon_cancel(eka2l1::ptr<epoc::request_status> logon_request, bool rendezvous) {
        decltype(rendezvous_requests) *container = rendezvous ? &rendezvous_requests : &logon_requests;

        const auto find_result = std::find(container->begin(), container->end(),epoc::notify_info(logon_request,
            kern->crr_thread()));

        if (find_result == container->end()) {
            return false;
        }

        find_result->complete(-3);
        container->erase(find_result);
        return true;
    }

    void process::rendezvous(int rendezvous_reason) {
        exit_reason = rendezvous_reason;
        exit_type = entity_exit_type::pending;

        for (auto &ren : rendezvous_requests) {
            ren.complete(rendezvous_reason);
            LOG_TRACE("Rendezvous to: {}", ren.requester->name());
        }

        rendezvous_requests.clear();
    }

    void process::finish_logons() {
        for (auto &req : logon_requests) {
            req.complete(exit_reason);
        }

        for (auto &req : rendezvous_requests) {
            req.complete(exit_reason);
        }

        logon_requests.clear();
        rendezvous_requests.clear();
    }

    void process::wait_dll_lock() {
        dll_lock->wait();
    }

    void process::signal_dll_lock(kernel::thread *callee) {
        dll_lock->signal(callee);
    }

    epoc::security_info process::get_sec_info() {
        return sec_info;
    }

    bool process::satisfy(epoc::security_policy &policy, epoc::security_info *missing) {
        // Do not enforce security on EKA1. It's not even there
        if (kern->is_eka1()) {
            return true;
        }

        [[maybe_unused]] epoc::security_info missing_holder;
        return policy.check(sec_info, missing ? *missing : missing_holder);
    }

    bool process::has(epoc::capability_set &cap_set) {
        // Do not enforce security on EKA1. It's not even there
        if (kern->is_eka1()) {
            return true;
        }

        return sec_info.has(cap_set);
    }

    void process::add_child_process(kernel::process *pr) {
        if (pr->parent_process_) {
            return;
        }

        auto ite = std::find(child_processes_.begin(), child_processes_.end(), pr);

        if (ite == child_processes_.end()) {
            pr->parent_process_ = this;
            child_processes_.push_back(pr);
        }
    }

    void process::detatch_from_parent() {
        if (parent_process_) {
            auto &child_array = parent_process_->child_processes_;
            auto ite = std::find(child_array.begin(), child_array.end(), this);

            if (child_array.end() != ite) {
                child_array.erase(ite);
            }

            parent_process_ = nullptr;
        }
    }

    void process::reload_compat_setting() {
        // Load preset for compability
        config::app_settings *settings = kern->get_app_settings();
        config::app_setting *individual_setting = settings->get_setting(std::get<2>(uids));

        if (individual_setting) {
            time_delay_ = individual_setting->time_delay;
            setting_inheritence_ = individual_setting->child_inherit_setting;
        } else {
            time_delay_ = 0;
        }
    }

    kernel::process *process::get_final_setting_process() {
        if (parent_process_ && parent_process_->setting_inheritence_) {
            return parent_process_->get_final_setting_process();
        }

        return this;
    }

    std::uint32_t process::get_time_delay() const {
        if (parent_process_ && parent_process_->setting_inheritence_) {
            return parent_process_->get_time_delay();
        }

        return time_delay_;
    }

    void process::set_time_delay(const std::uint32_t delay) {
        time_delay_ = delay;
    }
    
    void process::get_memory_info(memory_info &info) {
        info.rt_code_addr = codeseg->get_code_run_addr(this);
        info.rt_const_data_addr = codeseg->get_data_run_addr(this);
        info.rt_bss_addr = info.rt_const_data_addr;
        info.rt_bss_size = codeseg->get_bss_size();
        // TODO: More
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

        seri.absorb(std::get<0>(uids));
        seri.absorb(std::get<1>(uids));
        seri.absorb(std::get<2>(uids));

        kernel::uid prim_thread_id = primary_thread->unique_id();
        seri.absorb(prim_thread_id);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            primary_thread = kern->get_by_id<kernel::thread>(prim_thread_id);
        }

        seri.absorb(thread_count);
        seri.absorb(flags);
        seri.absorb(priority);
        seri.absorb(exit_reason);
        seri.absorb(exit_type);

        seri.absorb(process_name);
        seri.absorb(exe_path);
        seri.absorb(cmd_args);

        // We don't need to do state for page table, eventually it will be filled in by chunk
        // Do state for object table
        process_handles.do_state(seri);
    }
}

namespace eka2l1 {
    void *get_raw_pointer(kernel::process *pr, address addr) {
        return pr->get_ptr_on_addr_space(addr);
    }
}