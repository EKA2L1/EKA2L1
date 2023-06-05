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
#include <common/path.h>
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

        for (auto &pr_raw : kern->processes_) {
            kernel::process *pr = reinterpret_cast<kernel::process *>(pr_raw.get());
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

        // If this thread dies, processes booms
        primary_thread->set_process_permanent(true);
        dll_lock = kern->create<kernel::mutex>(kern->get_ntimer(), this, "dllLockMutexProcess" + common::to_string(std::get<2>(uids)),
            false, kernel::access_type::local_access);
    }

    void process::construct_with_codeseg(codeseg_ptr arg_codeseg, uint32_t stack_size, uint32_t heap_min, uint32_t heap_max,
        const process_priority pri) {
        if (codeseg) {
            return;
        }

        codeseg = std::move(arg_codeseg);
        uids = codeseg->get_uids();

        // Attach this codeseg to our process
        codeseg->attach(this);
        codeseg->unmark();

        codeseg->attached_report(this);

        // Get security info
        sec_info = codeseg->get_sec_info();
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

            LOG_INFO(KERNEL, "Process {} capabilities: {}", process_name, all_caps.empty() ? "None" : all_caps);
        }

        exe_path = codeseg->get_full_path();

        // Base on: sprocess.cpp#L245 in kernelhwsrv package
        create_prim_thread(codeseg->get_code_run_addr(this), codeseg->get_entry_point(this), stack_size, heap_min, heap_max,
            kernel::thread_priority::priority_normal);

        reload_compat_setting();

        // TODO: Load all references DLL in the export list.
    }

    static constexpr mem::vm_address get_rom_bss_addr(const mem::mem_model_type type, const bool mem_map_old, std::size_t &target_size) {
        switch (type) {
        case mem::mem_model_type::moving:
        case mem::mem_model_type::multiple:
            if (mem_map_old) {
                target_size = mem::dll_static_data_eka1_end - mem::rom_bss_eka1;
                return mem::rom_bss_eka1;
            } else {
                target_size = mem::shared_data - mem::dll_static_data;
                return mem::dll_static_data;
            }

        case mem::mem_model_type::flexible:
            target_size = mem::rom - mem::dll_static_data_flexible;
            return mem::dll_static_data_flexible;

        default:
            break;
        }

        return 0;
    }

    process_bss_man::process_bss_man(kernel_system *kern, process_ptr parent)
        : kern_(kern)
        , parent_(parent) {
    }

    process_bss_man::~process_bss_man() {
        for (auto &[addr, chunk] : bss_sects_) {
            kern_->destroy(chunk);
        }
    }

    chunk_ptr process_bss_man::make_bss_section_accordingly(const address addr) {
        // This is equals to what a page table can hold information on. On old model, a chunk is allocated with this granularity
        static constexpr std::uint32_t BSS_SECT_SIZE = 0x100000;

        memory_system *mem = kern_->get_memory_system();
        std::size_t total_size = 0;

        const address base = get_rom_bss_addr(mem->get_model_type(), kern_->is_eka1(), total_size);
        if ((addr < base) || (addr >= base + total_size)) {
            LOG_ERROR(KERNEL, "ROM BSS chunk has invalid address! (value=0x{:X})", addr);
            return nullptr;
        }

        // Round it up to a BSS section size
        const address new_addr = (addr & ~(BSS_SECT_SIZE - 1));
        auto bss_sect_res = bss_sects_.find(new_addr);

        if (bss_sect_res != bss_sects_.end()) {
            return bss_sect_res->second;
        }

        chunk_ptr rom_bss_chunk = kern_->create<kernel::chunk>(mem, parent_, fmt::format("RomBssChunkSect{:X}For{}", new_addr, parent_->unique_id()),
            0, 0, BSS_SECT_SIZE, prot_read_write, kernel::chunk_type::disconnected, kernel::chunk_access::dll_static_data,
            kernel::chunk_attrib::none, 0x00, false, new_addr);

        bss_sects_.emplace(new_addr, rom_bss_chunk);
        return rom_bss_chunk;
    }

    static bool is_process_uid_type_change_callback_elem_free(const process_uid_type_change_callback_elem &elem) {
        return !elem.first;
    }

    static void make_process_uid_type_change_callback_elem_free(process_uid_type_change_callback_elem &elem) {
        elem.first = nullptr;
    }

    process::process(kernel_system *kern, memory_system *mem, const std::string &process_name, const std::u16string &exe_path,
        const std::u16string &cmd_args, bool use_memory)
        : kernel_obj(kern, process_name, nullptr, access_type::local_access)
        , mm_impl_(nullptr)
        , dll_static_chunk(nullptr)
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
        , exit_category(u"None")
        , bss_man_(nullptr)
        , parent_process_(nullptr)
        , time_delay_(0)
        , setting_inheritence_(true)
        , generation_(0)
        , uid_change_callbacks(is_process_uid_type_change_callback_elem_free, make_process_uid_type_change_callback_elem_free) {
        obj_type = kernel::object_type::process;
        increase_access_count();

        if (!process_name.empty() && process_name.back() == '\0') {
            this->process_name.pop_back();
        }

        // Create mem model implementation
        if (use_memory) {
            mm_impl_ = mem::make_new_mem_model_process(mem->get_control(), mem->get_model_type());
        }

        generation_ = refresh_generation();
    }

    int process::destroy() {
        if (exit_type == entity_exit_type::pending) {
            kill(kernel::entity_exit_type::kill, u"Kill", 0);
        } else if (!kern->wipeout_in_progress()) {
            process_handles.reset();
        }

        return 0;
    }

    std::string process::name() const {
        std::string org = fmt::format("{}[{:08x}]", obj_name, std::get<2>(uids));
        if (!is_kernel_process()) {
            org += fmt::format("{:04x}", generation_ & 0xFFFF);
        }

        return org;
    }

    std::string process::raw_name() const {
        return obj_name;
    }

    void process::rename(const std::string &new_name) {
        kernel_obj::rename(new_name);
        generation_ = refresh_generation();
    }

    bool process::set_arg_slot(std::uint8_t slot, std::uint8_t *data, std::size_t data_size, const bool is_handle) {
        if ((slot >= 16) || args[slot].used) {
            return false;
        }

        args[slot].data.resize(data_size);
        args[slot].used = true;

        if (is_handle) {
            args[slot].obj = kern->get_kernel_obj_raw(*reinterpret_cast<kernel::handle*>(data), kern->crr_thread());
            args[slot].obj->increase_access_count();
        } else {
            args[slot].obj = nullptr;
        }

        std::copy(data, data + data_size, args[slot].data.begin());

        return true;
    }

    std::optional<kernel::handle> process::get_handle_from_arg_slot(const std::uint8_t slot, kernel::object_type obj_type, kernel::owner_type new_handle_owner) {
        kernel::kernel_obj *obj = nullptr;
        if (!parent_process_) {
            LOG_ERROR(KERNEL, "This process is a wild child, can't get handle from environment argument!");
            return std::nullopt;
        }

        if ((slot >= 16) || !args[slot].used || !args[slot].obj) {
            return std::nullopt;
        }

        obj = args[slot].obj;

        if (obj->get_object_type() != obj_type) {
            LOG_ERROR(KERNEL, "Handle points to object with wrong requested type! (objtype={}, requested={})", static_cast<int>(obj->get_object_type()), static_cast<int>(obj_type));
            return std::nullopt;
        }

        // Add it to our process
        kernel::handle h = kern->mirror(obj, new_handle_owner);
        obj->decrease_access_count();

        args[slot].obj = nullptr;
        args[slot].data.clear();
        args[slot].used = false;

        return h;
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
        process_uid_type old_type = uids;

        uids = std::move(type);
        generation_ = refresh_generation();

        reload_compat_setting();

        kern->run_uid_of_process_change_callback(this, old_type);

        for (auto &uid_change_callback : uid_change_callbacks) {
            uid_change_callback.second(uid_change_callback.first, uids);
        }
    }

    chunk_ptr process::get_rom_bss_chunk(const address addr) {
        if (!bss_man_) {
            bss_man_ = std::make_unique<process_bss_man>(kern, this);
        }

        return bss_man_->make_bss_section_accordingly(addr);
    }

    chunk_ptr process::get_dll_static_chunk(const address target_addr, const std::uint32_t size, std::uint32_t *offset) {
        if (!dll_static_chunk) {
            const std::uint32_t SIZE_STATIC_CHUNK = kern->is_eka1() ? (mem::dll_static_data_eka1_end - mem::dll_static_data_eka1)
                                                                    : (mem::shared_data - mem::dll_static_data);

            dll_static_chunk = kern->create<kernel::chunk>(mem, this, "", 0, 0, SIZE_STATIC_CHUNK,
                prot_read_write, kernel::chunk_type::disconnected, kernel::chunk_access::dll_static_data, kernel::chunk_attrib::anonymous,
                0x00);
        }

        if (target_addr < dll_static_chunk->base(this).ptr_address()) {
            LOG_ERROR(KERNEL, "Target address is invalid for DLL static");
            return nullptr;
        }

        const std::uint32_t target_off = target_addr - dll_static_chunk->base(this).ptr_address();

        if (!dll_static_chunk->commit(target_off, size)) {
            LOG_WARN(KERNEL, "Fail to commit DLL static data!");
        }

        if (offset) {
            *offset = target_off;
        }

        return dll_static_chunk;
    }

    std::size_t process::register_uid_type_change_callback(void *userdata, process_uid_type_change_callback callback) {
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

    void process::kill(const entity_exit_type ext, const std::u16string &category, const std::int32_t reason) {
        if (exit_type != entity_exit_type::pending) {
            return;
        }

        exit_type = ext;
        exit_reason = reason;
        exit_category = category;

        while (!thread_list.empty()) {
            kernel::thread *thr = E_LOFF(thread_list.first()->deque(), kernel::thread, process_thread_link);
            if (thr->exit_type == kernel::entity_exit_type::pending) {
                thr->kill(ext, u"Domino", reason);
            }
        }

        // Cleanup resources
        if (!kern->wipeout_in_progress()) {
            finish_logons();
            process_handles.reset();
        }

        kern->destroy(dll_lock);

        if (dll_static_chunk) {
            kern->destroy(dll_static_chunk);
        }

        while (!codeseg_list.empty()) {
            kernel::codeseg::attached_info *info = E_LOFF(codeseg_list.first()->deque(), kernel::codeseg::attached_info, process_link);
            info->parent_seg->detach(this, true);
        }

        mm_impl_.reset();
        bss_man_.reset();

        decrease_access_count();
    }

    void *process::get_ptr_on_addr_space(address addr) {
        if (!mm_impl_) {
            return nullptr;
        }

        return mem->get_control()->get_host_pointer(mm_impl_->address_space_id(), addr);
    }

    // EKA2L1 doesn't use multicore yet, so rendezvous and logon
    // are just simple.
    void process::logon(eka2l1::ptr<epoc::request_status> logon_request, bool rendezvous) {
        if (!thread_count) {
            (logon_request.get(kern->crr_process()))->set(exit_reason, kern->is_eka1());
            kern->crr_thread()->signal_request();

            return;
        }
        epoc::notify_info info(logon_request, kern->crr_thread());

        if (rendezvous) {
            if (handle_rendezvous_request(info)) {
                return;
            }

            rendezvous_requests.push_back(info);
            return;
        }

        logon_requests.push_back(info);
    }

    bool process::logon_cancel(eka2l1::ptr<epoc::request_status> logon_request, bool rendezvous) {
        decltype(rendezvous_requests) *container = rendezvous ? &rendezvous_requests : &logon_requests;

        const auto find_result = std::find(container->begin(), container->end(), epoc::notify_info(logon_request, kern->crr_thread()));

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
            LOG_TRACE(KERNEL, "Rendezvous to: {}", ren.requester->name());
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

        for (auto &req: logon_requests_emu) {
            req(this);
        }
    }

    void process::wait_dll_lock() {
        dll_lock->wait(kern->crr_thread());
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
            pr->parent_process_->increase_access_count();

            child_processes_.push_back(pr);
        }
    }

    bool process::has_child_process(kernel::process *pr) {
        return (std::find(child_processes_.begin(), child_processes_.end(), pr) != child_processes_.end());
    }

    void process::detatch_from_parent() {
        if (parent_process_) {
            auto &child_array = parent_process_->child_processes_;
            auto ite = std::find(child_array.begin(), child_array.end(), this);

            if (child_array.end() != ite) {
                child_array.erase(ite);
            }

            parent_process_->decrease_access_count();
            parent_process_ = nullptr;
        }
    }

    std::size_t process::logon(process_logon_callback callback) {
        return logon_requests_emu.add(callback);
    }

    void process::logon_cancel(const std::size_t handle) {
        logon_requests_emu.remove(handle);
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

    // NOTE: These two use global directory ASID (0) if no memory implementation exist or if it's kernel process.
    std::optional<std::uint32_t> process::read_dword_data_from(process *from_process, address addr) {
        mem::control_base *control = mem->get_control();
        return control->read_dword_data_from((from_process->mm_impl_ && !from_process->is_kernel_process()) ? from_process->mm_impl_->address_space_id() : 0,
            (mm_impl_ && !is_kernel_process()) ? mm_impl_->address_space_id() : 0, addr);
    }

    bool process::write_dword_data_to(process *to_process, address addr, const std::uint32_t target_data) {
        mem::control_base *control = mem->get_control();
        return control->write_dword_data_to((to_process->mm_impl_ && !to_process->is_kernel_process()) ? to_process->mm_impl_->address_space_id() : 0,
            (mm_impl_ && !is_kernel_process()) ? mm_impl_->address_space_id() : 0, addr, target_data);
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
