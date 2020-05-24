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

#include <epoc/utils/chunk.h>
#include <epoc/utils/des.h>
#include <epoc/utils/dll.h>
#include <epoc/utils/handle.h>
#include <epoc/utils/panic.h>
#include <epoc/utils/reqsts.h>
#include <epoc/utils/tl.h>
#include <epoc/utils/uid.h>

#include <epoc/dispatch/dispatcher.h>
#include <epoc/dispatch/screen.h>

#include <common/configure.h>
#include <epoc/common.h>
#include <epoc/dispatch/dispatcher.h>
#include <epoc/hal.h>
#include <epoc/svc.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>

#include <epoc/loader/rom.h>

#include <manager/config.h>
#include <manager/manager.h>

#ifdef ENABLE_SCRIPTING
#include <manager/script_manager.h>
#endif

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/random.h>
#include <common/time.h>
#include <common/types.h>

#if EKA2L1_PLATFORM(WIN32)
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Normaliz.lib")
#endif

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

#include <chrono>
#include <ctime>
#include <epoc/utils/err.h>

namespace eka2l1::epoc {
    static security_policy server_exclamation_point_name_policy({ cap_prot_serv });

    /* TODO:                                       
     * 1. (pent0) Implement global user data. Global user data should be allocated in global memory region.
    */

    /********************************/
    /*    GET/SET EXECUTIVE CALLS   */
    /*                              */
    /* Fast executive call, use for */
    /* get/set local data.          */
    /*                              */
    /********************************/

    /*! \brief Get the current heap allocator */
    BRIDGE_FUNC(eka2l1::ptr<void>, heap) {
        kernel::thread_local_data *local_data = current_local_data(sys);

        if (local_data->heap.ptr_address() == 0) {
            LOG_WARN("Allocator is not available.");
        }

        return local_data->heap;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, heap_switch, eka2l1::ptr<void> new_heap) {
        kernel::thread_local_data *local_data = current_local_data(sys);
        eka2l1::ptr<void> old_heap = local_data->heap;
        local_data->heap = new_heap;

        return old_heap;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, trap_handler) {
        kernel::thread_local_data *local_data = current_local_data(sys);
        return local_data->trap_handler;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, set_trap_handler, eka2l1::ptr<void> new_handler) {
        kernel::thread_local_data *local_data = current_local_data(sys);
        eka2l1::ptr<void> old_handler = local_data->trap_handler;
        local_data->trap_handler = new_handler;

        return local_data->trap_handler;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, active_scheduler) {
        kernel::thread_local_data *local_data = current_local_data(sys);
        return local_data->scheduler;
    }

    BRIDGE_FUNC(void, set_active_scheduler, eka2l1::ptr<void> new_scheduler) {
        kernel::thread_local_data *local_data = current_local_data(sys);
        local_data->scheduler = new_scheduler;
    }

    BRIDGE_FUNC(void, after, std::int32_t micro_secs, eka2l1::ptr<epoc::request_status> status) {
        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        thr->sleep_nof(status, micro_secs);
    }

    /****************************/
    /* PROCESS */
    /***************************/

    BRIDGE_FUNC(std::int32_t, process_exit_type, kernel::handle h) {
        eka2l1::memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(h);

        if (!pr_real) {
            LOG_ERROR("process_exit_type: Invalid process");
            return 0;
        }

        return static_cast<std::int32_t>(pr_real->get_exit_type());
    }

    BRIDGE_FUNC(void, process_rendezvous, std::int32_t complete_code) {
        kernel::process *pr = sys->get_kernel_system()->crr_process();
        pr->rendezvous(complete_code);
    }

    BRIDGE_FUNC(void, process_filename, std::int32_t process_handle, eka2l1::ptr<des8> name) {
        eka2l1::memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(process_handle);
        process_ptr cr_process = kern->crr_process();

        if (!pr_real) {
            LOG_ERROR("Svcprocess_filename: Invalid process");
            return;
        }

        epoc::des8 *des = name.get(cr_process);
        const std::u16string full_path_u16 = pr_real->get_exe_path();

        // Why???...
        const std::string full_path = common::ucs2_to_utf8(full_path_u16);
        des->assign(cr_process, full_path);
    }

    BRIDGE_FUNC(std::int32_t, process_get_memory_info, kernel::handle h, eka2l1::ptr<kernel::memory_info> info) {
        kernel::memory_info *info_host = info.get(sys->get_memory_system());
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(h);

        if (!pr_real) {
            return epoc::error_bad_handle;
        }

        pr_real->get_memory_info(*info_host);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, process_get_id, kernel::handle h) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(h);

        if (!pr_real) {
            LOG_ERROR("process_get_id: Invalid process");
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(pr_real->unique_id());
    }

    BRIDGE_FUNC(void, process_type, kernel::handle h, eka2l1::ptr<epoc::uid_type> uid_type) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(h);

        if (!pr_real) {
            LOG_ERROR("Svcprocess_type: Invalid process");
            return;
        }

        process_ptr crr_pr = kern->crr_process();

        epoc::uid_type *type = uid_type.get(crr_pr);
        auto tup = pr_real->get_uid_type();

        type->uid1 = std::get<0>(tup);
        type->uid2 = std::get<1>(tup);
        type->uid3 = std::get<2>(tup);
    }

    BRIDGE_FUNC(std::int32_t, process_data_parameter_length, std::int32_t slot) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::process *crr_process = kern->crr_process();

        if (slot >= 16 || slot < 0) {
            LOG_ERROR("Invalid slot (slot: {} >= 16 or < 0)", slot);
            return epoc::error_argument;
        }

        auto slot_ptr = crr_process->get_arg_slot(slot);

        if (!slot_ptr || !slot_ptr->used) {
            LOG_ERROR("Getting descriptor length of unused slot: {}", slot);
            return epoc::error_not_found;
        }

        return static_cast<std::int32_t>(slot_ptr->data.size());
    }

    BRIDGE_FUNC(std::int32_t, process_get_data_parameter, std::int32_t slot_num, eka2l1::ptr<std::uint8_t> data_ptr, std::int32_t length) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::process *pr = kern->crr_process();

        if (slot_num >= 16 || slot_num < 0) {
            LOG_ERROR("Invalid slot (slot: {} >= 16 or < 0)", slot_num);
            return epoc::error_argument;
        }

        auto slot = *pr->get_arg_slot(slot_num);

        if (!slot.used) {
            LOG_ERROR("Parameter slot unused, error: {}", slot_num);
            return epoc::error_not_found;
        }

        if (length < slot.data.size()) {
            LOG_ERROR("Given length is not large enough to slot length ({} vs {})",
                length, slot.data.size());
            return epoc::error_no_memory;
        }

        std::uint8_t *data = data_ptr.get(pr);
        std::copy(slot.data.begin(), slot.data.end(), data);

        std::size_t written_size = slot.data.size();
        pr->mark_slot_free(static_cast<std::uint8_t>(slot_num));

        return static_cast<std::int32_t>(written_size);
    }

    BRIDGE_FUNC(std::int32_t, process_set_data_parameter, kernel::handle h, std::int32_t slot_num, eka2l1::ptr<std::uint8_t> data, std::int32_t data_size) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        if (slot_num < 0 || slot_num >= 16) {
            LOG_ERROR("Invalid parameter slot: {}, slot number must be in range of 0-15", slot_num);
            return epoc::error_argument;
        }

        auto slot = *pr->get_arg_slot(slot_num);

        if (slot.used) {
            LOG_ERROR("Can't set parameter of an used slot: {}", slot_num);
            return epoc::error_in_use;
        }

        pr->set_arg_slot(slot_num, data.get(sys->get_memory_system()), data_size);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, process_command_line_length, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(pr->get_cmd_args().length());
    }

    BRIDGE_FUNC(void, process_command_line, kernel::handle h, eka2l1::ptr<epoc::des8> data_des) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            LOG_WARN("Process not found with handle: 0x{:x}", h);
            return;
        }

        epoc::des8 *data = data_des.get(sys->get_memory_system());

        if (!data) {
            return;
        }

        kernel::process *crr_process = kern->crr_process();

        std::u16string cmdline = pr->get_cmd_args();
        char *data_ptr = data->get_pointer(crr_process);

        memcpy(data_ptr, cmdline.data(), cmdline.length() << 1);
        data->set_length(crr_process, static_cast<std::uint32_t>(cmdline.length() << 1));
    }

    BRIDGE_FUNC(void, process_set_flags, kernel::handle h, std::uint32_t clear_mask, std::uint32_t set_mask) {
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr = kern->get<kernel::process>(h);

        uint32_t org_flags = pr->get_flags();
        uint32_t new_flags = ((org_flags & ~clear_mask) | set_mask);
        new_flags = (new_flags ^ org_flags);

        pr->set_flags(org_flags ^ new_flags);
    }

    BRIDGE_FUNC(std::int32_t, process_set_priority, kernel::handle h, std::int32_t process_priority) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        pr->set_priority(static_cast<eka2l1::kernel::process_priority>(process_priority));
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, process_rename, kernel::handle h, eka2l1::ptr<des8> new_name) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(h);
        process_ptr cur_pr = kern->crr_process();

        if (!pr) {
            return epoc::error_bad_handle;
        }

        const std::string new_name_str = new_name.get(cur_pr)->to_std_string(cur_pr);
        pr->rename(new_name_str);

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, process_resume, kernel::handle h) {
        process_ptr pr = sys->get_kernel_system()->get<kernel::process>(h);

        if (!pr) {
            return;
        }

        pr->run();
    }

    BRIDGE_FUNC(void, process_logon, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, bool rendezvous) {
        process_ptr pr = sys->get_kernel_system()->get<kernel::process>(h);

        if (!pr) {
            return;
        }

        pr->logon(req_sts, rendezvous);
    }

    BRIDGE_FUNC(std::int32_t, process_logon_cancel, kernel::handle h, eka2l1::ptr<epoc::request_status> request_sts, bool rendezvous) {
        process_ptr pr = sys->get_kernel_system()->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        bool logon_cancel_success = pr->logon_cancel(request_sts, rendezvous);

        if (logon_cancel_success) {
            return epoc::error_none;
        }

        return epoc::error_general;
    }

    /********************/
    /* TLS */
    /*******************/

    BRIDGE_FUNC(eka2l1::ptr<void>, dll_tls, kernel::handle h, std::int32_t dll_uid) {
        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        kernel::thread_local_data *dat = thr->get_local_data();

        for (const auto &tls : dat->tls_slots) {
            if (tls.handle == h) {
                return tls.pointer;
            }
        }

        LOG_WARN("TLS for 0x{:x}, thread {} return 0, may results unexpected crash", static_cast<std::uint32_t>(h),
            thr->name());

        return eka2l1::ptr<void>(0);
    }

    BRIDGE_FUNC(std::int32_t, dll_set_tls, kernel::handle h, std::int32_t dll_uid, eka2l1::ptr<void> data_set) {
        eka2l1::kernel::tls_slot *slot = get_tls_slot(sys, h);

        if (!slot) {
            return epoc::error_no_memory;
        }

        slot->pointer = data_set;

        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        LOG_TRACE("TLS set for 0x{:x}, ptr: 0x{:x}, thread {}", static_cast<std::uint32_t>(h), data_set.ptr_address(),
            thr->name());

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, dll_free_tls, kernel::handle h) {
        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        thr->close_tls_slot(*thr->get_tls_slot(h, h));

        LOG_TRACE("TLS slot closed for 0x{:x}, thread {}", static_cast<std::uint32_t>(h), thr->name());
    }

    BRIDGE_FUNC(void, dll_filename, std::int32_t entry_addr, eka2l1::ptr<epoc::des8> full_path_ptr) {
        std::optional<std::u16string> dll_full_path = get_dll_full_path(sys, entry_addr);

        if (!dll_full_path) {
            LOG_WARN("Unable to find DLL name for address: 0x{:x}", entry_addr);
            return;
        }

        std::string path_utf8 = common::ucs2_to_utf8(*dll_full_path);
        LOG_TRACE("Find DLL for address 0x{:x} with name: {}", static_cast<std::uint32_t>(entry_addr),
            path_utf8);

        kernel::process *crr_pr = sys->get_kernel_system()->crr_process();
        full_path_ptr.get(crr_pr)->assign(crr_pr, path_utf8);
    }

    /***********************************/
    /* LOCALE */
    /**********************************/

    /*
    * Warning: It's not possible to set the UTC time and offset in the emulator at the moment.
    */

    BRIDGE_FUNC(std::int32_t, utc_offset) {
        // TODO: Users and apps can set this
        return common::get_current_utc_offset();
    }

    enum : uint64_t {
        microsecs_per_sec = 1000000,
        ad_epoc_dist_microsecs = 62167132800 * microsecs_per_sec
    };

    // TODO (pent0): kernel's home time is currently not accurate enough.
    BRIDGE_FUNC(std::int32_t, time_now, eka2l1::ptr<std::uint64_t> time_ptr, eka2l1::ptr<std::int32_t> utc_offset_ptr) {
        kernel_system *kern = sys->get_kernel_system();

        std::uint64_t *time = time_ptr.get(kern->crr_process());
        std::int32_t *offset = utc_offset_ptr.get(kern->crr_process());

        const bool accurate_timing = sys->get_config()->accurate_ipc_timing;

        // The time is since EPOC, we need to convert it to first of AD
        *time = kern->home_time();
        *offset = common::get_current_utc_offset();

        return epoc::error_none;
    }

    /********************************************/
    /* IPC */
    /*******************************************/

    BRIDGE_FUNC(void, set_session_ptr, std::int32_t msg_handle, std::uint32_t session_addr) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        ipc_msg_ptr msg = kern->get_msg(msg_handle);

        if (!msg) {
            return;
        }

        msg->msg_session->set_cookie_address(session_addr);
    }

    BRIDGE_FUNC(std::int32_t, message_complete, std::int32_t msg_handle, std::int32_t val) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        ipc_msg_ptr msg = kern->get_msg(msg_handle);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        if (msg->request_sts) {
            *(msg->request_sts.get(msg->own_thr->owning_process())) = val;
            msg->own_thr->signal_request();
        }

        LOG_TRACE("Message completed with code: {}, thread to signal: {}", val, msg->own_thr->name());

#ifdef ENABLE_SCRIPTING
        // Invoke hook
        sys->get_manager_system()->get_script_manager()->call_ipc_complete(msg->msg_session->get_server()->name(),
            msg->function, msg.get());
#endif

        // Free the message
        msg->free = true;

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, message_kill, kernel::handle h, kernel::entity_exit_type etype, std::int32_t reason, eka2l1::ptr<desc8> cage) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        process_ptr crr = kern->crr_process();

        std::string exit_cage = cage.get(crr)->to_std_string(kern->crr_process());
        std::optional<std::string> exit_description;

        ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg || !msg->own_thr) {
            return epoc::error_bad_handle;
        }

        std::string thread_name = msg->own_thr->name();

        if (is_panic_category_action_default(exit_cage)) {
            exit_description = get_panic_description(exit_cage, reason);

            switch (etype) {
            case kernel::entity_exit_type::panic:
                LOG_TRACE("Thread {} panicked by message with category: {} and exit code: {} {}", thread_name, exit_cage, reason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            case kernel::entity_exit_type::kill:
                LOG_TRACE("Thread {} forcefully killed by message with category: {} and exit code: {}", thread_name, exit_cage, reason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            case kernel::entity_exit_type::terminate:
            case kernel::entity_exit_type::pending:
                LOG_TRACE("Thread {} terminated peacefully by message with category: {} and exit code: {}", thread_name, exit_cage, reason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            default:
                return epoc::error_argument;
            }
        }

#ifdef ENABLE_SCRIPTING
        manager::script_manager *scripter = sys->get_manager_system()->get_script_manager();
        scripter->call_panics(exit_cage, reason);
#endif
        process_ptr own_pr = msg->own_thr->owning_process();

        if (own_pr->decrease_thread_count() == 0) {
            own_pr->set_exit_type(etype);
        }

        kern->get_thread_scheduler()->stop(msg->own_thr);
        kern->prepare_reschedule();

        msg->own_thr->set_exit_type(etype);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, message_get_des_length, kernel::handle h, std::int32_t param) {
        if (param < 0) {
            return epoc::error_argument;
        }

        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        if ((int)msg->args.get_arg_type(param) & (int)ipc_arg_type::flag_des) {
            epoc::desc_base *base = eka2l1::ptr<epoc::desc_base>(msg->args.args[param]).get(msg->own_thr->owning_process());

            return base->get_length();
        }

        return epoc::error_bad_descriptor;
    }

    BRIDGE_FUNC(std::int32_t, message_get_des_max_length, kernel::handle h, std::int32_t param) {
        if (param < 0) {
            return epoc::error_argument;
        }

        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        const ipc_arg_type type = msg->args.get_arg_type(param);

        if ((int)type & (int)ipc_arg_type::flag_des) {
            kernel::process *own_pr = msg->own_thr->owning_process();
            return eka2l1::ptr<epoc::des8>(msg->args.args[param]).get(own_pr)->get_max_length(own_pr);
        }

        return epoc::error_bad_descriptor;
    }

    // TODO(pent0): This is inefficient code.
    BRIDGE_FUNC(std::int32_t, message_ipc_copy, kernel::handle h, std::int32_t param, eka2l1::ptr<ipc_copy_info> info,
        std::int32_t start_offset) {
        if (!info || param < 0) {
            return epoc::error_argument;
        }

        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        process_ptr crr_process = kern->crr_process();

        ipc_copy_info *info_host = info.get(crr_process);
        ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        bool des8 = true;
        if (info_host->flags & CHUNK_SHIFT_BY_1) {
            des8 = false;
        }

        bool read = true;
        if (info_host->flags & IPC_DIR_WRITE) {
            read = false;
        }

        if (read) {
            service::ipc_context context(false);
            context.sys = sys;
            context.msg = msg;

            if (des8) {
                const auto arg_request = context.get_arg<std::string>(param);

                if (!arg_request) {
                    return epoc::error_bad_descriptor;
                }

                const std::int32_t length_to_read = common::min(
                    static_cast<std::int32_t>(arg_request->length()) - start_offset, info_host->target_length);

                std::memcpy(info_host->target_ptr.get(crr_process), arg_request->data() + start_offset, length_to_read);
                return length_to_read;
            }

            const auto arg_request = context.get_arg<std::u16string>(param);

            if (!arg_request) {
                return epoc::error_bad_descriptor;
            }

            const std::int32_t length_to_read = common::min(
                static_cast<std::int32_t>(arg_request->length()) - start_offset, info_host->target_length);

            memcpy(info_host->target_ptr.get(crr_process), reinterpret_cast<const std::uint8_t *>(arg_request->data()) + start_offset * 2,
                length_to_read * 2);

            return length_to_read;
        }

        service::ipc_context context(false);
        context.sys = sys;
        context.msg = msg;

        std::string content;

        // We must keep the other part behind the offset
        if (des8) {
            content = std::move(*context.get_arg<std::string>(param));
        } else {
            std::u16string temp_content = *context.get_arg<std::u16string>(param);
            content.resize(temp_content.length() * 2);
            memcpy(&content[0], &temp_content[0], content.length());
        }

        std::uint32_t minimum_size = start_offset + info_host->target_length;
        des8 ? 0 : minimum_size *= 2;

        if (content.length() < minimum_size) {
            content.resize(minimum_size);
        }

        kernel::process *pr = kern->crr_process();

        memcpy(&content[des8 ? start_offset : start_offset * 2], info_host->target_ptr.get(pr),
            des8 ? info_host->target_length : info_host->target_length * 2);

        int error_code = 0;

        bool result = context.write_arg_pkg(param,
            reinterpret_cast<uint8_t *>(&content[0]), static_cast<std::uint32_t>(content.length()),
            &error_code);

        if (!result) {
            // -1 = bad descriptor, -2 = overflow
            if (error_code == -1) {
                return epoc::error_bad_descriptor;
            }

            return epoc::error_overflow;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, message_client, kernel::handle h, kernel::owner_type owner) {
        kernel_system *kern = sys->get_kernel_system();

        eka2l1::ipc_msg_ptr msg = kern->get_msg(h);
        if (!msg) {
            return epoc::error_bad_handle;
        }

        kernel::thread *msg_thr = msg->own_thr;
        return kern->open_handle(msg_thr, owner);
    }

    static void query_security_info(kernel::process *process, epoc::security_info *info) {
        assert(process);

        *info = std::move(process->get_sec_info());
    }

    BRIDGE_FUNC(void, process_security_info, kernel::handle h, eka2l1::ptr<epoc::security_info> info) {
        kernel_system *kern = sys->get_kernel_system();
        epoc::security_info *sec_info = info.get(kern->crr_process());

        process_ptr pr = kern->get<kernel::process>(h);
        query_security_info(&(*pr), sec_info);
    }

    BRIDGE_FUNC(void, thread_security_info, kernel::handle h, eka2l1::ptr<epoc::security_info> info) {
        kernel_system *kern = sys->get_kernel_system();
        epoc::security_info *sec_info = info.get(kern->crr_process());

        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR("Thread handle invalid 0x{:x}", h);
            return;
        }

        query_security_info(thr->owning_process(), sec_info);
    }

    BRIDGE_FUNC(void, message_security_info, std::int32_t h, eka2l1::ptr<epoc::security_info> info) {
        kernel_system *kern = sys->get_kernel_system();
        epoc::security_info *sec_info = info.get(kern->crr_process());

        eka2l1::ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            LOG_ERROR("Thread handle invalid 0x{:x}", h);
            return;
        }

        query_security_info(msg->own_thr->owning_process(), sec_info);
    }

    BRIDGE_FUNC(std::int32_t, server_create, eka2l1::ptr<desc8> server_name_des, std::int32_t mode) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::process *crr_pr = kern->crr_process();

        std::string server_name = server_name_des.get(crr_pr)->to_std_string(crr_pr);

        // Exclamination point at the beginning of server name requires ProtServ
        if (!server_name.empty() && server_name[0] == '!') {
            if (!crr_pr->satisfy(server_exclamation_point_name_policy)) {
                LOG_ERROR("Process {} try to create a server with exclamination point at the beginning of name ({}),"
                          " but doesn't have ProtServ",
                    crr_pr->name(), server_name);

                return epoc::error_permission_denied;
            }
        }

        auto handle = kern->create_and_add<service::server>(kernel::owner_type::process,
                              server_name)
                          .first;

        if (handle != INVALID_HANDLE) {
            LOG_TRACE("Server {} created", server_name);
        }

        return handle;
    }

    BRIDGE_FUNC(void, server_receive, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, eka2l1::ptr<void> data_ptr) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        server_ptr server = kern->get<service::server>(h);

        if (!server) {
            return;
        }

        LOG_TRACE("Receive requested from {}", server->name());

        server->receive_async_lle(req_sts, data_ptr.cast<service::message2>());
    }

    BRIDGE_FUNC(void, server_cancel, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        server_ptr server = kern->get<service::server>(h);

        if (!server) {
            return;
        }

        server->cancel_async_lle();
    }

    static std::int32_t do_create_session_from_server(system *sys, server_ptr server, std::int32_t msg_slot_count, eka2l1::ptr<void> sec, std::int32_t mode) {
        kernel_system *kern = sys->get_kernel_system();
        auto session_and_handle = kern->create_and_add<service::session>(
            kernel::owner_type::process, server, msg_slot_count);

        if (session_and_handle.first == INVALID_HANDLE) {
            return epoc::error_general;
        }

        LOG_TRACE("New session connected to {} with handle {}", server->name(), session_and_handle.first);
        session_and_handle.second->set_associated_handle(session_and_handle.first);

        return session_and_handle.first;
    }

    BRIDGE_FUNC(std::int32_t, session_create, eka2l1::ptr<desc8> server_name_des, std::int32_t msg_slot, eka2l1::ptr<void> sec, std::int32_t mode) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr = kern->crr_process();

        const std::string server_name = server_name_des.get(pr)->to_std_string(pr);
        server_ptr server = kern->get_by_name<service::server>(server_name);

        if (!server) {
            LOG_TRACE("Create session to unexist server: {}", server_name);
            return epoc::error_not_found;
        }

        return do_create_session_from_server(sys, server, msg_slot, sec, mode);
    }

    BRIDGE_FUNC(std::int32_t, session_create_from_handle, kernel::handle h, std::int32_t msg_slots, eka2l1::ptr<void> sec, std::int32_t mode) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr = kern->crr_process();
        server_ptr server = kern->get<service::server>(h);

        if (!server) {
            LOG_TRACE("Create session to unexist server handle: {}", h);
            return epoc::error_not_found;
        }

        return do_create_session_from_server(sys, server, msg_slots, sec, mode);
    }

    BRIDGE_FUNC(std::int32_t, session_share, std::uint32_t *handle, std::int32_t share) {
        kernel_system *kern = sys->get_kernel_system();
        session_ptr ss = kern->get<service::session>(*handle);

        if (!ss) {
            return epoc::error_bad_handle;
        }

        if (share == 2) {
            // Explicit attach: other process uses IPC can open this handle, so do threads! :D
            ss->set_access_type(kernel::access_type::global_access);
        } else {
            ss->set_access_type(kernel::access_type::local_access);
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, session_send_sync, kernel::handle h, std::int32_t ord, eka2l1::ptr<void> ipc_args,
        eka2l1::ptr<epoc::request_status> status) {
        // LOG_TRACE("Send using handle: {}", (h & 0x8000) ? (h & ~0x8000) : (h));

        kernel_system *kern = sys->get_kernel_system();
        process_ptr crr_pr = kern->crr_process();

        // Dispatch the header
        ipc_arg arg;
        std::int32_t *arg_header = ipc_args.cast<std::int32_t>().get(crr_pr);

        if (ipc_args) {
            for (uint8_t i = 0; i < 4; i++) {
                arg.args[i] = *arg_header++;
            }

            arg.flag = *arg_header & (((1 << 12) - 1) | (int)ipc_arg_pin::pin_mask);
        }

        session_ptr ss = kern->get<service::session>(h);

        if (!ss) {
            return epoc::error_bad_handle;
        }

        if (!status) {
            LOG_TRACE("Sending a blind sync message");
        }

        if (sys->get_config()->log_ipc) {
            LOG_TRACE("Sending {} sync to {}", ord, ss->get_server()->name());
        }

#ifdef ENABLE_SCRIPTING
        manager::script_manager *scripter = sys->get_manager_system()->get_script_manager();
        scripter->call_ipc_send(ss->get_server()->name(),
            ord, arg.args[0], arg.args[1], arg.args[2], arg.args[3], arg.flag,
            sys->get_kernel_system()->crr_thread());
#endif

        const int result = ss->send_receive_sync(ord, arg, status);

        if (ss->get_server()->is_hle()) {
            // Process it right away.
            ss->get_server()->process_accepted_msg();
        }

        return result;
    }

    BRIDGE_FUNC(std::int32_t, session_send, kernel::handle h, std::int32_t ord, eka2l1::ptr<void> ipc_args,
        eka2l1::ptr<epoc::request_status> status) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr crr_pr = kern->crr_process();

        // Dispatch the header
        ipc_arg arg;
        std::int32_t *arg_header = ipc_args.cast<std::int32_t>().get(crr_pr);

        if (ipc_args) {
            for (uint8_t i = 0; i < 4; i++) {
                arg.args[i] = *arg_header++;
            }

            arg.flag = *arg_header & (((1 << 12) - 1) | (int)ipc_arg_pin::pin_mask);
        }

        session_ptr ss = kern->get<service::session>(h);

        if (!ss) {
            return epoc::error_bad_handle;
        }

        if (!status) {
            LOG_TRACE("Sending a blind async message");
        }

        if (sys->get_config()->log_ipc) {
            LOG_TRACE("Sending {} to {}", ord, ss->get_server()->name());
        }

#ifdef ENABLE_SCRIPTING
        manager::script_manager *scripter = sys->get_manager_system()->get_script_manager();
        scripter->call_ipc_send(ss->get_server()->name(),
            ord, arg.args[0], arg.args[1], arg.args[2], arg.args[3], arg.flag,
            sys->get_kernel_system()->crr_thread());
#endif

        const int result = ss->send_receive(ord, arg, status);

        if (ss->get_server()->is_hle()) {
            // Process it right away.
            ss->get_server()->process_accepted_msg();
        }

        return result;
    }

    /**********************************/
    /* TRAP/LEAVE */
    /*********************************/

    BRIDGE_FUNC(eka2l1::ptr<void>, leave_start) {
        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        LOG_CRITICAL("Leave started! Guess leave code: {}", static_cast<std::int32_t>(sys->get_cpu()->get_reg(0)));

        thr->increase_leave_depth();

        return current_local_data(sys)->trap_handler;
    }

    BRIDGE_FUNC(void, leave_end) {
        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        thr->decrease_leave_depth();

        if (thr->is_invalid_leave()) {
            LOG_CRITICAL("Invalid leave, leave depth is negative!");
        }

        LOG_TRACE("Leave trapped by trap handler.");
    }

    BRIDGE_FUNC(std::int32_t, debug_mask) {
        return 0;
    }

    BRIDGE_FUNC(void, set_debug_mask, std::int32_t ts_debug_mask) {
        // Doing nothing here
    }

    BRIDGE_FUNC(std::int32_t, debug_mask_index, std::int32_t idx) {
        return 0;
    }

    BRIDGE_FUNC(std::int32_t, hal_function, std::int32_t cage, std::int32_t func, eka2l1::ptr<std::int32_t> a1,
        eka2l1::ptr<std::int32_t> a2) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->crr_process();

        int *arg1 = a1.get(pr);
        int *arg2 = a2.get(pr);

        return do_hal(sys, cage, func, arg1, arg2);
    }

    /**********************************/
    /* CHUNK */
    /*********************************/

    BRIDGE_FUNC(std::int32_t, chunk_new, epoc::owner_type owner, eka2l1::ptr<desc8> name_des,
        eka2l1::ptr<epoc::chunk_create> chunk_create_info_ptr) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->crr_process();

        epoc::chunk_create create_info = *chunk_create_info_ptr.get(pr);
        desc8 *name = name_des.get(pr);

        kernel::chunk_type type = kernel::chunk_type::normal;
        kernel::chunk_access access = kernel::chunk_access::local;
        kernel::chunk_attrib att = decltype(att)::none;
        prot perm = prot::read_write;

        // Fetch chunk type
        if (create_info.att & epoc::chunk_create::disconnected) {
            type = kernel::chunk_type::disconnected;
        } else if (create_info.att & epoc::chunk_create::double_ended) {
            type = kernel::chunk_type::double_ended;
        }

        // Fetch chunk access
        if (create_info.att & epoc::chunk_create::global) {
            access = kernel::chunk_access::global;
        } else {
            access = kernel::chunk_access::local;
        }

        if (create_info.att & epoc::chunk_create::code) {
            perm = prot::read_write_exec;
        }

        if ((access == decltype(access)::global) && ((!name) || (name->get_length() == 0))) {
            att = kernel::chunk_attrib::anonymous;
        }

        const kernel::handle h = kern->create_and_add<kernel::chunk>(
                                         owner == epoc::owner_process ? kernel::owner_type::process : kernel::owner_type::thread,
                                         sys->get_memory_system(), kern->crr_process(), name ? name->to_std_string(kern->crr_process()) : "", create_info.initial_bottom,
                                         create_info.initial_top, create_info.max_size, perm, type, access, att)
                                     .first;

        if (h == INVALID_HANDLE) {
            return epoc::error_no_memory;
        }

        return h;
    }

    BRIDGE_FUNC(std::int32_t, chunk_max_size, kernel::handle h) {
        chunk_ptr chunk = sys->get_kernel_system()->get<kernel::chunk>(h);
        if (!chunk) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(chunk->max_size());
    }

    BRIDGE_FUNC(eka2l1::ptr<std::uint8_t>, chunk_base, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        chunk_ptr chunk = kern->get<kernel::chunk>(h);
        if (!chunk) {
            return 0;
        }

        return chunk->base(kern->crr_process());
    }

    BRIDGE_FUNC(std::int32_t, chunk_size, kernel::handle h) {
        chunk_ptr chunk = sys->get_kernel_system()->get<kernel::chunk>(h);
        if (!chunk) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(chunk->committed());
    }

    BRIDGE_FUNC(std::int32_t, chunk_adjust, kernel::handle h, std::int32_t type, std::int32_t a1, std::int32_t a2) {
        chunk_ptr chunk = sys->get_kernel_system()->get<kernel::chunk>(h);

        if (!chunk) {
            return epoc::error_bad_handle;
        }

        auto fetch = [type](chunk_ptr chunk, int a1, int a2) -> bool {
            switch (type) {
            case 0:
                return chunk->adjust(a1);

            case 1:
                return chunk->adjust_de(a1, a2);

            case 2:
                return chunk->commit(a1, a2);

            case 3:
                return chunk->decommit(a1, a2);

            case 4: // Allocate. Afaik this adds more commit size
                return chunk->allocate(a1);

            case 5:
            case 6:
                return true;
            }

            return false;
        };

        bool res = fetch(chunk, a1, a2);

        if (!res) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, imb_range, eka2l1::ptr<void> addr, std::uint32_t size) {
        sys->get_cpu()->imb_range(addr.ptr_address(), size);
    }

    /********************/
    /* SYNC PRIMITIVES  */
    /********************/

    BRIDGE_FUNC(std::int32_t, semaphore_create, eka2l1::ptr<desc8> sema_name_des, std::int32_t init_count, epoc::owner_type owner) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->crr_process();

        desc8 *desname = sema_name_des.get(pr);
        kernel::owner_type owner_kern = (owner == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread;

        const kernel::handle sema = kern->create_and_add<kernel::semaphore>(owner_kern, !desname ? "" : desname->to_std_string(pr).c_str(),
                                            init_count, !desname ? kernel::access_type::local_access : kernel::access_type::global_access)
                                        .first;

        if (sema == INVALID_HANDLE) {
            return epoc::error_general;
        }

        return sema;
    }

    BRIDGE_FUNC(std::int32_t, semaphore_wait, kernel::handle h, std::int32_t timeout) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        sema_ptr sema = kern->get<kernel::semaphore>(h);

        if (!sema) {
            return epoc::error_bad_handle;
        }

        if (timeout) {
            LOG_WARN("Semaphore timeout unimplemented");
        }

        sema->wait();
        return epoc::error_none;
    }

    BRIDGE_FUNC(void, semaphore_signal, kernel::handle h) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        sema_ptr sema = kern->get<kernel::semaphore>(h);

        if (!sema) {
            return;
        }

        sema->signal(1);
    }

    BRIDGE_FUNC(void, semaphore_signal_n, kernel::handle h, std::int32_t sig_count) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        sema_ptr sema = kern->get<kernel::semaphore>(h);

        if (!sema) {
            return;
        }

        sema->signal(sig_count);
    }

    BRIDGE_FUNC(std::int32_t, mutex_create, eka2l1::ptr<desc8> mutex_name_des, epoc::owner_type owner) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->crr_process();

        desc8 *desname = mutex_name_des.get(pr);
        kernel::owner_type owner_kern = (owner == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread;

        const kernel::handle mut = kern->create_and_add<kernel::mutex>(owner_kern, sys->get_ntimer(),
                                           !desname ? "" : desname->to_std_string(pr), false,
                                           !desname ? kernel::access_type::local_access : kernel::access_type::global_access)
                                       .first;

        if (mut == INVALID_HANDLE) {
            return epoc::error_general;
        }

        return mut;
    }

    BRIDGE_FUNC(std::int32_t, mutex_wait, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        mutex_ptr mut = kern->get<kernel::mutex>(h);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return epoc::error_bad_handle;
        }

        mut->wait();
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, mutex_wait_ver2, kernel::handle h, std::int32_t timeout) {
        kernel_system *kern = sys->get_kernel_system();
        mutex_ptr mut = kern->get<kernel::mutex>(h);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return epoc::error_bad_handle;
        }

        if (timeout == 0) {
            mut->wait();
            return epoc::error_none;
        }

        if (timeout == -1) {
            // Try lock
            mut->try_wait();
            return epoc::error_none;
        }

        mut->wait_for(timeout);
        return epoc::error_none;
    }

    BRIDGE_FUNC(void, mutex_signal, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        mutex_ptr mut = kern->get<kernel::mutex>(h);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return;
        }

        mut->signal(kern->crr_thread());
    }

    BRIDGE_FUNC(std::int32_t, mutex_is_held, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        mutex_ptr mut = kern->get<kernel::mutex>(h);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return epoc::error_not_found;
        }

        const bool result = (mut->holder() == kern->crr_thread());

        return result;
    }

    BRIDGE_FUNC(void, wait_for_any_request) {
        sys->get_kernel_system()->crr_thread()->wait_for_any_request();
    }

    BRIDGE_FUNC(void, request_signal, std::int32_t signal_count) {
        sys->get_kernel_system()->crr_thread()->signal_request(signal_count);
    }

    /***********************************************/
    /* HANDLE FUNCTIONS   */
    /*                    */
    /* Thread independent */
    /**********************************************/

    BRIDGE_FUNC(std::int32_t, object_next, std::int32_t obj_type, eka2l1::ptr<des8> name_des, eka2l1::ptr<epoc::find_handle> handle_finder) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr = kern->crr_process();

        epoc::find_handle *handle = handle_finder.get(pr);
        std::string name = name_des.get(pr)->to_std_string(pr);

        LOG_TRACE("Finding object name: {}", name);

        std::optional<eka2l1::find_handle> info = kern->find_object(name, handle->handle,
            static_cast<kernel::object_type>(obj_type), true);

        if (!info) {
            return epoc::error_not_found;
        }

        handle->handle = info->index;
        handle->obj_id_low = static_cast<uint32_t>(info->object_id);

        // We are never gonna reached the high part
        handle->obj_id_high = 0;

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, handle_close, kernel::handle h) {
        if (h & 0x8000) {
            return epoc::error_general;
        }

        int res = sys->get_kernel_system()->close(h);
        return res;
    }

    BRIDGE_FUNC(std::int32_t, handle_duplicate, std::int32_t h, epoc::owner_type owner, std::int32_t dup_handle) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        return kern->mirror(&(*kern->get<kernel::thread>(h)), dup_handle,
            (owner == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread);
    }

    BRIDGE_FUNC(std::int32_t, handle_duplicate_v2, std::int32_t h, epoc::owner_type owner, std::uint32_t *dup_handle) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        const std::uint32_t handle_result = kern->mirror(&(*kern->get<kernel::thread>(h)), *dup_handle,
            (owner == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread);

        if (handle_result == INVALID_HANDLE) {
            return epoc::error_not_found;
        }

        *dup_handle = handle_result;
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, handle_open_object, std::int32_t obj_type, eka2l1::ptr<epoc::desc8> name_des, std::int32_t owner) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();
        process_ptr pr = kern->crr_process();

        std::string obj_name = name_des.get(pr)->to_std_string(pr);

        // What a waste if we find the ID already but not mirror it
        kernel_obj_ptr obj = kern->get_by_name_and_type<kernel::kernel_obj>(obj_name,
            static_cast<kernel::object_type>(obj_type));

        if (!obj) {
            LOG_ERROR("Can't open object: {}", obj_name);
            return epoc::error_not_found;
        }

        kernel::handle ret_handle = kern->mirror(obj, static_cast<eka2l1::kernel::owner_type>(owner));

        if (ret_handle != INVALID_HANDLE) {
            return ret_handle;
        }

        return epoc::error_general;
    }

    BRIDGE_FUNC(void, handle_name, kernel::handle h, eka2l1::ptr<des8> name_des) {
        kernel_system *kern = sys->get_kernel_system();
        kernel_obj_ptr obj = kern->get_kernel_obj_raw(h);
        process_ptr crr_pr = kern->crr_process();

        if (!obj) {
            return;
        }

        des8 *desname = name_des.get(crr_pr);
        desname->assign(crr_pr, obj->name());
    }

    BRIDGE_FUNC(void, handle_full_name, kernel::handle h, eka2l1::ptr<des8> full_name_des) {
        kernel_system *kern = sys->get_kernel_system();
        kernel_obj_ptr obj = kern->get_kernel_obj_raw(h);
        process_ptr pr = kern->crr_process();

        if (!obj) {
            return;
        }

        des8 *desname = full_name_des.get(pr);

        std::string full_name;
        obj->full_name(full_name);

        desname->assign(pr, full_name);
    }

    BRIDGE_FUNC(void, handle_count, kernel::handle h, std::uint32_t *total_handle_process, std::uint32_t *total_handle_thread) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return;
        }

        *total_handle_thread = static_cast<std::uint32_t>(thr->get_total_open_handles());
        *total_handle_process = static_cast<std::uint32_t>(thr->owning_process()->get_total_open_handles());
    }

    /******************************/
    /* CODE SEGMENT */
    /*****************************/

    BRIDGE_FUNC(std::int32_t, static_call_list, eka2l1::ptr<std::int32_t> total_ptr, eka2l1::ptr<std::uint32_t> list_ptr_guest) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::process *pr = kern->crr_process();

        std::uint32_t *list_ptr = list_ptr_guest.get(pr);
        std::int32_t *total = total_ptr.get(pr);

        std::vector<uint32_t> list;
        pr->get_codeseg()->queries_call_list(pr, list);
        pr->get_codeseg()->unmark();

        *total = static_cast<std::int32_t>(list.size());
        memcpy(list_ptr, list.data(), sizeof(std::uint32_t) * *total);

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, static_call_done) {
    }

    BRIDGE_FUNC(std::int32_t, wait_dll_lock) {
        sys->get_kernel_system()->crr_process()->wait_dll_lock();
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, release_dll_lock) {
        sys->get_kernel_system()->crr_process()->signal_dll_lock(
            sys->get_kernel_system()->crr_thread());

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, library_attach, kernel::handle h, eka2l1::ptr<std::int32_t> num_eps, eka2l1::ptr<std::uint32_t> ep_list) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        library_ptr lib = kern->get<kernel::library>(h);

        if (!lib) {
            return epoc::error_bad_handle;
        }

        process_ptr pr = kern->crr_process();

        std::vector<uint32_t> entries = lib->attach(kern->crr_process());
        const std::uint32_t num_to_copy = common::min<std::uint32_t>(*num_eps.get(pr),
            static_cast<std::uint32_t>(entries.size()));

        *num_eps.get(pr) = num_to_copy;

        address *episode_choose_your_story = ep_list.cast<address>().get(pr);
        std::memcpy(episode_choose_your_story, entries.data(), num_to_copy * sizeof(address));

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, library_lookup, kernel::handle h, std::uint32_t ord_index) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        library_ptr lib = kern->get<kernel::library>(h);

        if (!lib) {
            return 0;
        }

        std::optional<uint32_t> func_addr = lib->get_ordinal_address(kern->crr_process(),
            ord_index);

        if (!func_addr) {
            return 0;
        }

        return *func_addr;
    }

    BRIDGE_FUNC(std::int32_t, library_attached, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        library_ptr lib = kern->get<kernel::library>(h);

        if (!lib) {
            return epoc::error_bad_handle;
        }

        bool attached_result = lib->attached();

        if (!attached_result) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    // Note: Guess name.
    BRIDGE_FUNC(std::uint32_t, library_load_prepare) {
        return epoc::error_none;
    }
    
    BRIDGE_FUNC(std::uint32_t, library_entry_call_start, const address addr) {
        return epoc::error_none;
    }

    /************************/
    /* USER SERVER */
    /***********************/

    BRIDGE_FUNC(std::int32_t, user_svr_rom_header_address) {
        return sys->get_rom_info()->header.rom_base;
    }

    BRIDGE_FUNC(std::int32_t, user_svr_rom_root_dir_address) {
        return sys->get_rom_info()->header.rom_root_dir_list;
    }

    /************************/
    /*  THREAD  */
    /************************/

    struct thread_create_info_expand {
        int handle;
        int type;
        address func_ptr;
        address ptr;
        address supervisor_stack;
        int supervisor_stack_size;
        address user_stack;
        int user_stack_size;
        kernel::thread_priority init_thread_priority;
        ptr_desc<std::uint8_t> name;
        int total_size;

        address allocator;
        int heap_initial_size;
        int heap_max_size;
        int flags;
    };

    static_assert(sizeof(thread_create_info_expand) == 64,
        "Thread create info struct size invalid");

    BRIDGE_FUNC(std::int32_t, thread_create, eka2l1::ptr<desc8> thread_name_des, epoc::owner_type owner, eka2l1::ptr<thread_create_info_expand> info_ptr) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        process_ptr pr = kern->crr_process();

        // Get rid of null terminator
        std::string thr_name = thread_name_des.get(pr)->to_std_string(pr).c_str();
        thread_create_info_expand *info = info_ptr.get(pr);

        if (thr_name.empty()) {
            thr_name = "AnonymousThread";
        }

        const kernel::handle thr_handle = kern->create_and_add<kernel::thread>(static_cast<kernel::owner_type>(owner),
                                                  mem, kern->get_ntimer(), kern->crr_process(),
                                                  kernel::access_type::local_access, thr_name, info->func_ptr, info->user_stack_size,
                                                  info->heap_initial_size, info->heap_max_size, false, info->ptr, info->allocator, kernel::thread_priority::priority_normal)
                                              .first;

        if (thr_handle == INVALID_HANDLE) {
            return epoc::error_general;
        } else {
            LOG_TRACE("Thread {} created with start pc = 0x{:x}, stack size = 0x{:x}", thr_name,
                info->func_ptr, info->user_stack_size);
        }

        return thr_handle;
    }

    BRIDGE_FUNC(std::int32_t, last_thread_handle) {
        return sys->get_kernel_system()->crr_thread()->last_handle();
    }

    BRIDGE_FUNC(std::int32_t, thread_id, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(thr->unique_id());
    }

    BRIDGE_FUNC(std::int32_t, thread_kill, kernel::handle h, kernel::entity_exit_type etype, std::int32_t reason, eka2l1::ptr<desc8> reason_des) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        std::string exit_cage = "None";
        std::string thread_name = thr->name();

        kernel::process *pr = kern->crr_process();

        if (reason_des) {
            exit_cage = reason_des.get(pr)->to_std_string(pr);
        }

        std::optional<std::string> exit_description;

        if (is_panic_category_action_default(exit_cage)) {
            exit_description = get_panic_description(exit_cage, reason);

            switch (etype) {
            case kernel::entity_exit_type::panic:
                LOG_TRACE("Thread {} panicked with category: {} and exit code: {} {}", thread_name, exit_cage, reason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            case kernel::entity_exit_type::kill:
                LOG_TRACE("Thread {} forcefully killed with category: {} and exit code: {} {}", thread_name, exit_cage, reason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            case kernel::entity_exit_type::terminate:
            case kernel::entity_exit_type::pending:
                LOG_TRACE("Thread {} terminated peacefully with category: {} and exit code: {}", thread_name, exit_cage, reason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            default:
                return epoc::error_argument;
            }
        }

        if (thr->owning_process()->decrease_thread_count() == 0) {
            thr->owning_process()->set_exit_type(etype);
        }

#ifdef ENABLE_SCRIPTING
        manager::script_manager *scripter = sys->get_manager_system()->get_script_manager();
        scripter->call_panics(exit_cage, reason);
#endif

        kern->get_thread_scheduler()->stop(&(*thr));
        kern->prepare_reschedule();

        thr->set_exit_type(etype);

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, thread_request_signal, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR("Thread not found with handle {}", h);
            return;
        }

        thr->signal_request();
    }

    BRIDGE_FUNC(std::int32_t, thread_rename, kernel::handle h, eka2l1::ptr<desc8> name_des) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        process_ptr pr = kern->crr_process();

        thread_ptr thr = kern->get<kernel::thread>(h);
        desc8 *name = name_des.get(pr);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        std::string new_name = name->to_std_string(pr);

        LOG_TRACE("Thread with last name: {} renamed to {}", thr->name(), new_name);

        thr->rename(new_name);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, thread_process, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(h);

        return kern->mirror(kern->get_by_id<kernel::process>(thr->owning_process()->unique_id()),
            kernel::owner_type::thread);
    }

    BRIDGE_FUNC(void, thread_set_priority, kernel::handle h, std::int32_t thread_pri) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return;
        }

        thr->set_priority(static_cast<eka2l1::kernel::thread_priority>(thread_pri));
    }

    BRIDGE_FUNC(void, thread_resume, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR("invalid thread handle 0x{:x}", h);
            return;
        }

        switch (thr->current_state()) {
        case kernel::thread_state::create: {
            kern->get_thread_scheduler()->schedule(&(*thr));
            break;
        }

        default: {
            thr->resume();
            break;
        }
        }
    }

    BRIDGE_FUNC(void, thread_suspend, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR("invalid thread handle 0x{:x}", h);
            return;
        }

        switch (thr->current_state()) {
        case kernel::thread_state::create: {
            break;
        }

        default: {
            thr->suspend();
            break;
        }
        }
    }

    BRIDGE_FUNC(void, thread_rendezvous, std::int32_t reason) {
        sys->get_kernel_system()->crr_thread()->rendezvous(reason);
    }

    BRIDGE_FUNC(void, thread_logon, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, bool rendezvous) {
        thread_ptr thr = sys->get_kernel_system()->get<kernel::thread>(h);

        if (!thr) {
            return;
        }

        thr->logon(req_sts, rendezvous);
    }

    BRIDGE_FUNC(std::int32_t, thread_logon_cancel, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts,
        bool rendezvous) {
        thread_ptr thr = sys->get_kernel_system()->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        bool logon_success = thr->logon_cancel(req_sts, rendezvous);

        if (logon_success) {
            return epoc::error_none;
        }

        return epoc::error_general;
    }

    BRIDGE_FUNC(void, thread_set_flags, kernel::handle h, std::uint32_t clear_mask, std::uint32_t set_mask) {
        kernel_system *kern = sys->get_kernel_system();

        thread_ptr thr = kern->get<kernel::thread>(h);

        uint32_t org_flags = thr->get_flags();
        uint32_t new_flags = ((org_flags & ~clear_mask) | set_mask);
        new_flags = (new_flags ^ org_flags);

        thr->set_flags(org_flags ^ new_flags);
    }

    BRIDGE_FUNC(std::int32_t, thread_open_by_id, const std::uint32_t id, const epoc::owner_type owner) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::thread *thr = kern->get_by_id<kernel::thread>(id);

        if (!thr) {
            LOG_ERROR("Unable to find thread with ID: {}", id);
            return epoc::error_not_found;
        }

        const kernel::handle h = kern->open_handle_with_thread(kern->crr_thread(), thr,
            static_cast<kernel::owner_type>(owner));

        if (h == INVALID_HANDLE) {
            return epoc::error_general;
        }

        return static_cast<std::int32_t>(h);
    }

    BRIDGE_FUNC(std::int32_t, thread_exit_type, const kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_not_found;
        }

        return static_cast<std::int32_t>(thr->get_exit_type());
    }

    /*****************************/
    /* PROPERTY */
    /****************************/

    BRIDGE_FUNC(std::int32_t, property_find_get_int, std::int32_t cage, std::int32_t key, eka2l1::ptr<std::int32_t> value) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        property_ptr prop = kern->get_prop(cage, key);

        if (!prop) {
            LOG_WARN("Property not found: category = 0x{:x}, key = 0x{:x}", cage, key);
            return epoc::error_not_found;
        }

        std::int32_t *val_ptr = value.get(kern->crr_process());
        *val_ptr = prop->get_int();

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_find_get_bin, std::int32_t cage, std::int32_t key, eka2l1::ptr<std::uint8_t> data, std::int32_t datlength) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr crr_pr = kern->crr_process();

        property_ptr prop = kern->get_prop(cage, key);

        if (!prop) {
            LOG_WARN("Property not found: category = 0x{:x}, key = 0x{:x}", cage, key);
            return epoc::error_not_found;
        }

        std::uint8_t *data_ptr = data.get(crr_pr);
        auto data_vec = prop->get_bin();

        const std::size_t size_to_copy = std::min<std::size_t>(data_vec.size(), datlength);
        std::int32_t return_code = epoc::error_none;

        if (data_vec.size() > datlength) {
            // The given buffer can't hold ours.
            return_code = epoc::error_overflow;
        }

        // Whether the buffer is too small, we still have to either copy truncated or full data.
        std::copy(data_vec.begin(), data_vec.begin() + size_to_copy, data.get(kern->crr_process()));

        if (return_code != epoc::error_none) {
            return return_code;
        }

        return datlength;
    }

    BRIDGE_FUNC(std::int32_t, property_attach, std::int32_t cage, std::int32_t val, epoc::owner_type owner) {
        kernel_system *kern = sys->get_kernel_system();
        property_ptr prop = kern->get_prop(cage, val);

        LOG_TRACE("Attach to property with category: 0x{:x}, key: 0x{:x}", cage, val);

        if (!prop) {
            prop = kern->create<service::property>();

            if (!prop) {
                return epoc::error_general;
            }

            prop->first = cage;
            prop->second = val;
        }

        auto property_ref_handle_and_obj = kern->create_and_add<service::property_reference>(
            static_cast<kernel::owner_type>(owner), prop);

        if (property_ref_handle_and_obj.first == INVALID_HANDLE) {
            return epoc::error_general;
        }

        return property_ref_handle_and_obj.first;
    }

    BRIDGE_FUNC(std::int32_t, property_define, std::int32_t cage, std::int32_t key, eka2l1::ptr<epoc::property_info> prop_info_ptr) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->crr_process();

        epoc::property_info *info = prop_info_ptr.get(pr);

        service::property_type prop_type;

        switch (info->type) {
        case property_type_int:
            prop_type = service::property_type::int_data;
            break;

        case property_type_large_byte_array:
        case property_type_byte_array:
            prop_type = service::property_type::bin_data;
            break;

        default: {
            LOG_WARN("Unknown property type, exit with epoc::error_general.");
            return epoc::error_argument;
        }
        }

        LOG_TRACE("Define to property with category: 0x{:x}, key: 0x{:x}, type: {}", cage, key,
            prop_type == service::property_type::int_data ? "int" : "bin");

        property_ptr prop = kern->get_prop(cage, key);

        if (!prop) {
            prop = kern->create<service::property>();

            if (!prop) {
                return epoc::error_general;
            }

            prop->first = cage;
            prop->second = key;
        }

        prop->define(prop_type, info->size);

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, property_subscribe, kernel::handle h, eka2l1::ptr<epoc::request_status> sts) {
        kernel_system *kern = sys->get_kernel_system();
        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop) {
            return;
        }

        epoc::notify_info info(sts, kern->crr_thread());
        prop->subscribe(info);
    }

    BRIDGE_FUNC(void, property_cancel, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop) {
            return;
        }

        prop->cancel();

        return;
    }

    BRIDGE_FUNC(std::int32_t, property_set_int, kernel::handle h, std::int32_t val) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        bool res = prop->get_property_object()->set_int(val);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_set_bin, kernel::handle h, std::int32_t size, eka2l1::ptr<std::uint8_t> data_ptr) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        bool res = prop->get_property_object()->set(data_ptr.get(kern->crr_process()), size);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_get_int, kernel::handle h, eka2l1::ptr<std::int32_t> value_ptr) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->crr_process();

        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        *value_ptr.get(pr) = prop->get_property_object()->get_int();

        if (prop->get_property_object()->get_int() == -1) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_get_bin, kernel::handle h, eka2l1::ptr<std::uint8_t> buffer_ptr_guest, std::int32_t buffer_size) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        std::vector<uint8_t> dat = prop->get_property_object()->get_bin();

        if (dat.size() == 0) {
            return epoc::error_argument;
        }

        const std::size_t size_to_copy = std::min<std::size_t>(dat.size(), buffer_size);
        std::int32_t return_code = epoc::error_none;

        if (dat.size() > buffer_size) {
            // The given buffer can't hold ours.
            return_code = epoc::error_overflow;
        }

        // Whether the buffer is too small, we still have to either copy truncated or full data.
        std::copy(dat.begin(), dat.begin() + size_to_copy, buffer_ptr_guest.get(kern->crr_process()));

        if (return_code != epoc::error_none) {
            return return_code;
        }

        return buffer_size;
    }

    BRIDGE_FUNC(std::int32_t, property_find_set_int, std::int32_t cage, std::int32_t key, std::int32_t value) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ptr prop = kern->get_prop(cage, key);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        const bool res = prop->set(value);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_find_set_bin, std::int32_t cage, std::int32_t key, eka2l1::ptr<std::uint8_t> data_ptr, std::int32_t size) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ptr prop = kern->get_prop(cage, key);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        const bool res = prop->set(data_ptr.get(kern->crr_process()), size);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    /**********************/
    /* TIMER */
    /*********************/
    BRIDGE_FUNC(void, clear_inactivity_time) {
    }

    BRIDGE_FUNC(std::int32_t, timer_create) {
        return sys->get_kernel_system()->create_and_add<kernel::timer>(
                                           kernel::owner_type::process, sys->get_ntimer(),
                                           "timer" + common::to_string(eka2l1::random()))
            .first;
    }

    /* 
    * Note: the difference between At and After on hardware is At request still actives when the phone shutdown.
    * At is extremely important to implement the alarm in S60 (i believe S60v4 is a part based on S60 so it maybe related).
    * In emulator, it's the same, so i implement it as TimerAffter.
    */

    BRIDGE_FUNC(void, timer_after, kernel::handle h, eka2l1::ptr<epoc::request_status> aRequestStatus, std::int32_t aMicroSeconds) {
        kernel_system *kern = sys->get_kernel_system();
        timer_ptr timer = kern->get<kernel::timer>(h);

        if (!timer) {
            return;
        }

        epoc::request_status *sts = aRequestStatus.get(kern->crr_process());
        timer->after(kern->crr_thread(), sts, aMicroSeconds);
    }

    BRIDGE_FUNC(void, timer_at_utc, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, std::uint64_t us_at) {
        kernel_system *kern = sys->get_kernel_system();
        timer_ptr timer = kern->get<kernel::timer>(h);

        if (!timer) {
            return;
        }

        const bool accurate_timing = sys->get_config()->accurate_ipc_timing;

        timer->after(kern->crr_thread(), req_sts.get(kern->crr_process()), us_at - (accurate_timing ? kern->home_time() : common::get_current_time_in_microseconds_since_1ad()));
    }

    BRIDGE_FUNC(void, timer_cancel, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        timer_ptr timer = kern->get<kernel::timer>(h);

        if (!timer) {
            return;
        }

        timer->cancel_request();
    }

    BRIDGE_FUNC(std::uint32_t, ntick_count) {
        const std::uint64_t DEFAULT_NTICK_PERIOD = (microsecs_per_sec / epoc::NANOKERNEL_HZ);

        ntimer *timing = sys->get_ntimer();
        return static_cast<std::uint32_t>(timing->microseconds() / DEFAULT_NTICK_PERIOD);
    }

    BRIDGE_FUNC(std::uint32_t, tick_count) {
        const std::uint64_t DEFAULT_TICK_PERIOD = (common::microsecs_per_sec / epoc::TICK_TIMER_HZ);

        ntimer *timing = sys->get_ntimer();
        return static_cast<std::uint32_t>(timing->microseconds() / DEFAULT_TICK_PERIOD);
    }

    /**********************/
    /* CHANGE NOTIFIER */
    /**********************/
    BRIDGE_FUNC(std::int32_t, change_notifier_create, epoc::owner_type owner) {
        return sys->get_kernel_system()->create_and_add<kernel::change_notifier>(static_cast<kernel::owner_type>(owner)).first;
    }

    BRIDGE_FUNC(std::int32_t, change_notifier_logon, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        change_notifier_ptr cnot = kern->get<kernel::change_notifier>(h);

        if (!cnot) {
            return epoc::error_bad_handle;
        }

        bool res = cnot->logon(req_sts);

        if (!res) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, change_notifier_logoff, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        change_notifier_ptr cnot = kern->get<kernel::change_notifier>(h);

        if (!cnot) {
            return epoc::error_bad_handle;
        }

        bool res = cnot->logon_cancel();

        if (!res) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    /* MESSAGE QUEUE */
    BRIDGE_FUNC(std::int32_t, message_queue_notify_data_available, const kernel::handle h, eka2l1::ptr<epoc::request_status> sts) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::thread *request_thread = kern->crr_thread();

        epoc::notify_info info;
        info.requester = request_thread;
        info.sts = sts;

        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(h);

        if (!queue) {
            return epoc::error_bad_handle;
        }

        const bool result = queue->notify_available(info);

        if (!result) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, message_queue_cancel_notify_available, kernel::handle h) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::thread *request_thread = kern->crr_thread();

        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(h);

        if (!queue) {
            return;
        }

        queue->cancel_data_available(request_thread);
    }

    BRIDGE_FUNC(std::int32_t, message_queue_send, kernel::handle h, void *data, const std::int32_t length) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(h);

        if (!queue) {
            return epoc::error_bad_handle;
        }

        if ((length <= 0) || (static_cast<std::size_t>(length) > queue->max_message_length())) {
            return epoc::error_argument;
        }

        if (!queue->send(data, length)) {
            return epoc::error_overflow;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, message_queue_receive, kernel::handle h, void *data, const std::int32_t length) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(h);

        if (!queue) {
            return epoc::error_bad_handle;
        }

        if (length != queue->max_message_length()) {
            return epoc::error_argument;
        }

        if (!queue->receive(data, length)) {
            return epoc::error_underflow;
        }

        return epoc::error_none;
    }

    /* DEBUG AND SECURITY */

    BRIDGE_FUNC(void, debug_print, eka2l1::ptr<desc8> aDes, std::int32_t aMode) {
        LOG_TRACE("{}",
            aDes.get(sys->get_memory_system())->to_std_string(sys->get_kernel_system()->crr_process()));
    }

    BRIDGE_FUNC(std::int32_t, btrace_out, const std::uint32_t a0, const std::uint32_t a1, const std::uint32_t a2,
        const std::uint32_t a3) {
        kernel_system *kern = sys->get_kernel_system();
        manager::config_state *conf = sys->get_config();

        if (!conf->enable_btrace) {
            // Passed
            return 1;
        }

        return static_cast<std::int32_t>(kern->get_btrace()->out(a0, a1, a2, a3));
    }

    // Let all pass for now
    BRIDGE_FUNC(std::int32_t, plat_sec_diagnostic, eka2l1::ptr<void> plat_sec_info) {
        return epoc::error_none;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, get_global_userdata) {
        //LOG_INFO("get_global_userdata stubbed with zero");
        return 0;
    }

    BRIDGE_FUNC(address, exception_descriptor, address in_addr) {
        return epoc::get_exception_descriptor_addr(sys, in_addr);
    }

    BRIDGE_FUNC(address, exception_handler, kernel::handle h) {
        kernel::thread *thr = sys->get_kernel_system()->get<kernel::thread>(h);

        if (!thr) {
            return 0;
        }

        return thr->get_exception_handler();
    }

    BRIDGE_FUNC(std::int32_t, set_exception_handler, kernel::handle h, address handler, std::uint32_t mask) {
        kernel::thread *thr = sys->get_kernel_system()->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        thr->set_exception_handler(handler);
        thr->set_exception_mask(mask);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, is_exception_handled, kernel::handle h, std::int32_t type, bool aSwExcInProgress) {
        LOG_ERROR("Exception with type {} is thrown", type);
        kernel::thread *thr = sys->get_kernel_system()->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        if (thr->get_exception_handler() == 0) {
            return 0;
        }

        return 1;
    }

    /* ATOMIC OPERATION */
    /* TODO: Use host atomic function when multi-core available */
    struct SAtomicOpInfo32 {
        void *iA;
        union {
            void *iQ;
            std::uint32_t i0;
        };

        std::uint32_t i1;
        std::uint32_t i2;
    };

    BRIDGE_FUNC(std::int32_t, safe_inc_32, eka2l1::ptr<std::int32_t> val_ptr) {
        kernel_system *kern = sys->get_kernel_system();
        std::int32_t *val = val_ptr.get(kern->crr_process());
        std::int32_t org_val = *val;
        *val > 0 ? val++ : 0;

        return org_val;
    }

    BRIDGE_FUNC(std::int32_t, safe_dec_32, eka2l1::ptr<std::int32_t> val_ptr) {
        kernel_system *kern = sys->get_kernel_system();
        std::int32_t *val = val_ptr.get(kern->crr_process());
        std::int32_t org_val = *val;
        *val > 0 ? val-- : 0;

        return org_val;
    }

    BRIDGE_FUNC(std::int32_t, SafeDec32, eka2l1::ptr<std::int32_t> aVal) {
        std::int32_t *val = aVal.get(sys->get_memory_system());
        std::int32_t org_val = *val;
        *val > 0 ? val-- : 0;

        return org_val;
    }

    /// HLE
    BRIDGE_FUNC(void, hle_dispatch, const std::uint32_t ordinal) {
        sys->get_dispatcher()->resolve(sys, ordinal);
    }

    BRIDGE_FUNC(void, virtual_reality) {
        // Call host function. Hack.
        typedef bool (*reality_func)(void *data);

        const std::uint32_t current = sys->get_cpu()->get_pc();
        std::uint64_t *data = reinterpret_cast<std::uint64_t *>(sys->get_kernel_system()->crr_process()->get_ptr_on_addr_space(current - 20));

        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        sys->get_cpu()->save_context(thr->get_thread_context());

        reality_func to_call = reinterpret_cast<reality_func>(*data++);
        void *userdata = reinterpret_cast<void *>(*data++);

        if (!to_call(userdata)) {
            sys->get_kernel_system()->crr_thread()->wait_for_any_request();
        }
    }

    BRIDGE_FUNC(std::uint32_t, math_rand) {
        return eka2l1::random();
    }

    BRIDGE_FUNC(void, add_event, raw_event *evt) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();

        switch (evt->type_) {
        case raw_event_type_redraw:
            dispatcher->update_all_screens(sys);
            break;

        default:
            LOG_WARN("Unhandled raw event {}", static_cast<int>(evt->type_));
            break;
        }
    }

    const eka2l1::hle::func_map svc_register_funcs_v10 = {
        /* FAST EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00800000, wait_for_any_request),
        BRIDGE_REGISTER(0x00800001, heap),
        BRIDGE_REGISTER(0x00800002, heap_switch),
        BRIDGE_REGISTER(0x00800005, active_scheduler),
        BRIDGE_REGISTER(0x00800006, set_active_scheduler),
        BRIDGE_REGISTER(0x00800008, trap_handler),
        BRIDGE_REGISTER(0x00800009, set_trap_handler),
        BRIDGE_REGISTER(0x0080000A, debug_mask),
        BRIDGE_REGISTER(0x0080000B, debug_mask_index),
        BRIDGE_REGISTER(0x00800011, user_svr_rom_header_address),
        BRIDGE_REGISTER(0x00800012, user_svr_rom_root_dir_address),
        BRIDGE_REGISTER(0x00800015, utc_offset),
        BRIDGE_REGISTER(0x00800016, get_global_userdata),
        BRIDGE_REGISTER(0x00800030, hle_dispatch),
        /* SLOW EXECUTIVE CALL */
        BRIDGE_REGISTER(0x01, chunk_base),
        BRIDGE_REGISTER(0x02, chunk_size),
        BRIDGE_REGISTER(0x03, chunk_max_size),
        BRIDGE_REGISTER(0x0E, library_lookup),
        BRIDGE_REGISTER(0x15, process_resume),
        BRIDGE_REGISTER(0x16, process_filename),
        BRIDGE_REGISTER(0x17, process_command_line),
        BRIDGE_REGISTER(0x1E, process_set_flags),
        BRIDGE_REGISTER(0x1F, semaphore_wait),
        BRIDGE_REGISTER(0x20, semaphore_signal),
        BRIDGE_REGISTER(0x21, semaphore_signal_n),
        BRIDGE_REGISTER(0x22, server_receive),
        BRIDGE_REGISTER(0x23, server_cancel),
        BRIDGE_REGISTER(0x24, set_session_ptr),
        BRIDGE_REGISTER(0x25, session_send),
        BRIDGE_REGISTER(0x27, session_share),
        BRIDGE_REGISTER(0x28, thread_resume),
        BRIDGE_REGISTER(0x29, thread_suspend),
        BRIDGE_REGISTER(0x2B, thread_set_priority),
        BRIDGE_REGISTER(0x2F, thread_set_flags),
        BRIDGE_REGISTER(0x3A, request_signal),
        BRIDGE_REGISTER(0x3B, handle_name),
        BRIDGE_REGISTER(0x3C, handle_full_name),
        BRIDGE_REGISTER(0x41, message_complete),
        BRIDGE_REGISTER(0x4D, session_send_sync),
        BRIDGE_REGISTER(0x4F, hal_function),
        BRIDGE_REGISTER(0x52, process_command_line_length),
        BRIDGE_REGISTER(0x56, debug_print),
        BRIDGE_REGISTER(0x5A, exception_handler),
        BRIDGE_REGISTER(0x5E, is_exception_handled),
        BRIDGE_REGISTER(0x64, process_type),
        BRIDGE_REGISTER(0x68, thread_create),
        BRIDGE_REGISTER(0x6A, handle_close),
        BRIDGE_REGISTER(0x6B, chunk_new),
        BRIDGE_REGISTER(0x6C, chunk_adjust),
        BRIDGE_REGISTER(0x6D, handle_open_object),
        BRIDGE_REGISTER(0x6E, handle_duplicate_v2),
        BRIDGE_REGISTER(0x6F, mutex_create),
        BRIDGE_REGISTER(0x70, semaphore_create),
        BRIDGE_REGISTER(0x73, thread_kill),
        BRIDGE_REGISTER(0x78, thread_rename),
        BRIDGE_REGISTER(0x7F, server_create),
        BRIDGE_REGISTER(0x80, session_create),
        BRIDGE_REGISTER(0x9D, wait_dll_lock),
        BRIDGE_REGISTER(0x9E, release_dll_lock),
        BRIDGE_REGISTER(0x9F, library_attach),
        BRIDGE_REGISTER(0xA0, library_attached),
        BRIDGE_REGISTER(0xA1, static_call_list),
        BRIDGE_REGISTER(0xA9, message_ipc_copy),
        BRIDGE_REGISTER(0xAF, process_security_info),
        BRIDGE_REGISTER(0xBD, property_define),
        BRIDGE_REGISTER(0xBF, property_attach),
        BRIDGE_REGISTER(0xC0, property_subscribe),
        BRIDGE_REGISTER(0xC1, property_cancel),
        BRIDGE_REGISTER(0xC6, property_find_get_int),
        BRIDGE_REGISTER(0xC7, property_find_get_bin),
        BRIDGE_REGISTER(0xC8, property_find_set_int),
        BRIDGE_REGISTER(0xD2, process_get_data_parameter),
        BRIDGE_REGISTER(0xD3, process_data_parameter_length),
        BRIDGE_REGISTER(0xDE, thread_request_signal),
        BRIDGE_REGISTER(0xDD, exception_descriptor),
        BRIDGE_REGISTER(0xE0, leave_start),
        BRIDGE_REGISTER(0xE1, leave_end),
        BRIDGE_REGISTER(0xE9, btrace_out),
        BRIDGE_REGISTER(0x10B, static_call_done),
        BRIDGE_REGISTER(0x10C, library_entry_call_start),
        BRIDGE_REGISTER(0x10D, library_load_prepare)
    };

    const eka2l1::hle::func_map svc_register_funcs_v94 = {
        /* FAST EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00800000, wait_for_any_request),
        BRIDGE_REGISTER(0x00800001, heap),
        BRIDGE_REGISTER(0x00800002, heap_switch),
        BRIDGE_REGISTER(0x00800005, active_scheduler),
        BRIDGE_REGISTER(0x00800006, set_active_scheduler),
        BRIDGE_REGISTER(0x00800008, trap_handler),
        BRIDGE_REGISTER(0x00800009, set_trap_handler),
        BRIDGE_REGISTER(0x0080000C, debug_mask),
        BRIDGE_REGISTER(0x0080000D, debug_mask_index),
        BRIDGE_REGISTER(0x0080000E, set_debug_mask),
        BRIDGE_REGISTER(0x00800010, ntick_count),
        BRIDGE_REGISTER(0x00800013, user_svr_rom_header_address),
        BRIDGE_REGISTER(0x00800014, user_svr_rom_root_dir_address),
        BRIDGE_REGISTER(0x00800015, safe_inc_32),
        BRIDGE_REGISTER(0x00800016, safe_dec_32),
        BRIDGE_REGISTER(0x00800019, utc_offset),
        BRIDGE_REGISTER(0x0080001A, get_global_userdata),
        BRIDGE_REGISTER(0x00800030, hle_dispatch),

        /* SLOW EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00, object_next),
        BRIDGE_REGISTER(0x01, chunk_base),
        BRIDGE_REGISTER(0x02, chunk_size),
        BRIDGE_REGISTER(0x03, chunk_max_size),
        BRIDGE_REGISTER(0x05, tick_count),
        BRIDGE_REGISTER(0x0B, math_rand),
        BRIDGE_REGISTER(0x0C, imb_range),
        BRIDGE_REGISTER(0x0E, library_lookup),
        BRIDGE_REGISTER(0x11, mutex_wait),
        BRIDGE_REGISTER(0x12, mutex_signal),
        BRIDGE_REGISTER(0x13, process_get_id),
        BRIDGE_REGISTER(0x14, dll_filename),
        BRIDGE_REGISTER(0x15, process_resume),
        BRIDGE_REGISTER(0x16, process_filename),
        BRIDGE_REGISTER(0x17, process_command_line),
        BRIDGE_REGISTER(0x18, process_exit_type),
        BRIDGE_REGISTER(0x1C, process_set_priority),
        BRIDGE_REGISTER(0x1E, process_set_flags),
        BRIDGE_REGISTER(0x1F, semaphore_wait),
        BRIDGE_REGISTER(0x20, semaphore_signal),
        BRIDGE_REGISTER(0x21, semaphore_signal_n),
        BRIDGE_REGISTER(0x22, server_receive),
        BRIDGE_REGISTER(0x23, server_cancel),
        BRIDGE_REGISTER(0x24, set_session_ptr),
        BRIDGE_REGISTER(0x25, session_send),
        BRIDGE_REGISTER(0x26, thread_id),
        BRIDGE_REGISTER(0x27, session_share),
        BRIDGE_REGISTER(0x28, thread_resume),
        BRIDGE_REGISTER(0x29, thread_suspend),
        BRIDGE_REGISTER(0x2B, thread_set_priority),
        BRIDGE_REGISTER(0x2F, thread_set_flags),
        BRIDGE_REGISTER(0x31, thread_exit_type),
        BRIDGE_REGISTER(0x35, timer_cancel),
        BRIDGE_REGISTER(0x36, timer_after),
        BRIDGE_REGISTER(0x37, timer_at_utc),
        BRIDGE_REGISTER(0x39, change_notifier_logon),
        BRIDGE_REGISTER(0x3A, change_notifier_logoff),
        BRIDGE_REGISTER(0x3B, request_signal),
        BRIDGE_REGISTER(0x3C, handle_name),
        BRIDGE_REGISTER(0x3D, handle_full_name),
        BRIDGE_REGISTER(0x3F, handle_count),
        BRIDGE_REGISTER(0x40, after),
        BRIDGE_REGISTER(0x42, message_complete),
        BRIDGE_REGISTER(0x44, time_now),
        BRIDGE_REGISTER(0x4D, session_send_sync),
        BRIDGE_REGISTER(0x4E, dll_tls),
        BRIDGE_REGISTER(0x4F, hal_function),
        BRIDGE_REGISTER(0x52, process_command_line_length),
        BRIDGE_REGISTER(0x55, clear_inactivity_time),
        BRIDGE_REGISTER(0x56, debug_print),
        BRIDGE_REGISTER(0x5A, exception_handler),
        BRIDGE_REGISTER(0x5B, set_exception_handler),
        BRIDGE_REGISTER(0x5E, is_exception_handled),
        BRIDGE_REGISTER(0x5F, process_get_memory_info),
        BRIDGE_REGISTER(0x6A, handle_close),
        BRIDGE_REGISTER(0x64, process_type),
        BRIDGE_REGISTER(0x68, thread_create),
        BRIDGE_REGISTER(0x6B, chunk_new),
        BRIDGE_REGISTER(0x6C, chunk_adjust),
        BRIDGE_REGISTER(0x6D, handle_open_object),
        BRIDGE_REGISTER(0x6E, handle_duplicate),
        BRIDGE_REGISTER(0x6F, mutex_create),
        BRIDGE_REGISTER(0x70, semaphore_create),
        BRIDGE_REGISTER(0x71, thread_open_by_id),
        BRIDGE_REGISTER(0x73, thread_kill),
        BRIDGE_REGISTER(0x74, thread_logon),
        BRIDGE_REGISTER(0x75, thread_logon_cancel),
        BRIDGE_REGISTER(0x76, dll_set_tls),
        BRIDGE_REGISTER(0x77, dll_free_tls),
        BRIDGE_REGISTER(0x78, thread_rename),
        BRIDGE_REGISTER(0x79, process_rename),
        BRIDGE_REGISTER(0x7B, process_logon),
        BRIDGE_REGISTER(0x7C, process_logon_cancel),
        BRIDGE_REGISTER(0x7D, thread_process),
        BRIDGE_REGISTER(0x7E, server_create),
        BRIDGE_REGISTER(0x7F, session_create),
        BRIDGE_REGISTER(0x80, session_create_from_handle),
        BRIDGE_REGISTER(0x84, timer_create),
        BRIDGE_REGISTER(0x86, after), // Actually AfterHighRes
        BRIDGE_REGISTER(0x87, change_notifier_create),
        BRIDGE_REGISTER(0x9C, wait_dll_lock),
        BRIDGE_REGISTER(0x9D, release_dll_lock),
        BRIDGE_REGISTER(0x9E, library_attach),
        BRIDGE_REGISTER(0x9F, library_attached),
        BRIDGE_REGISTER(0xA0, static_call_list),
        BRIDGE_REGISTER(0xA3, last_thread_handle),
        BRIDGE_REGISTER(0xA4, thread_rendezvous),
        BRIDGE_REGISTER(0xA5, process_rendezvous),
        BRIDGE_REGISTER(0xA6, message_get_des_length),
        BRIDGE_REGISTER(0xA7, message_get_des_max_length),
        BRIDGE_REGISTER(0xA8, message_ipc_copy),
        BRIDGE_REGISTER(0xA9, message_client),
        BRIDGE_REGISTER(0xAC, message_kill),
        BRIDGE_REGISTER(0xAE, process_security_info),
        BRIDGE_REGISTER(0xAF, thread_security_info),
        BRIDGE_REGISTER(0xB0, message_security_info),
        BRIDGE_REGISTER(0xB5, message_queue_send),
        BRIDGE_REGISTER(0xB6, message_queue_receive),
        BRIDGE_REGISTER(0xB9, message_queue_notify_data_available),
        BRIDGE_REGISTER(0xBA, message_queue_cancel_notify_available),
        BRIDGE_REGISTER(0xBC, property_define),
        BRIDGE_REGISTER(0xBE, property_attach),
        BRIDGE_REGISTER(0xBF, property_subscribe),
        BRIDGE_REGISTER(0xC0, property_cancel),
        BRIDGE_REGISTER(0xC1, property_get_int),
        BRIDGE_REGISTER(0xC2, property_get_bin),
        BRIDGE_REGISTER(0xC3, property_set_int),
        BRIDGE_REGISTER(0xC4, property_set_bin),
        BRIDGE_REGISTER(0xC5, property_find_get_int),
        BRIDGE_REGISTER(0xC6, property_find_get_bin),
        BRIDGE_REGISTER(0xC7, property_find_set_int),
        BRIDGE_REGISTER(0xC8, property_find_set_bin),
        BRIDGE_REGISTER(0xCF, process_set_data_parameter),
        BRIDGE_REGISTER(0xD1, process_get_data_parameter),
        BRIDGE_REGISTER(0xD2, process_data_parameter_length),
        BRIDGE_REGISTER(0xDB, plat_sec_diagnostic),
        BRIDGE_REGISTER(0xDC, exception_descriptor),
        BRIDGE_REGISTER(0xDD, thread_request_signal),
        BRIDGE_REGISTER(0xDE, mutex_is_held),
        BRIDGE_REGISTER(0xDF, leave_start),
        BRIDGE_REGISTER(0xE0, leave_end),
        BRIDGE_REGISTER(0xE8, btrace_out)
    };

    const eka2l1::hle::func_map svc_register_funcs_v93 = {
        /* FAST EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00800000, wait_for_any_request),
        BRIDGE_REGISTER(0x00800001, heap),
        BRIDGE_REGISTER(0x00800002, heap_switch),
        BRIDGE_REGISTER(0x00800005, active_scheduler),
        BRIDGE_REGISTER(0x00800006, set_active_scheduler),
        BRIDGE_REGISTER(0x00800008, trap_handler),
        BRIDGE_REGISTER(0x00800009, set_trap_handler),
        BRIDGE_REGISTER(0x0080000D, debug_mask),
        BRIDGE_REGISTER(0x00800010, ntick_count),
        BRIDGE_REGISTER(0x00800013, user_svr_rom_header_address),
        BRIDGE_REGISTER(0x00800014, user_svr_rom_root_dir_address),
        BRIDGE_REGISTER(0x00800015, safe_inc_32),
        BRIDGE_REGISTER(0x00800016, safe_dec_32),
        BRIDGE_REGISTER(0x00800019, utc_offset),
        BRIDGE_REGISTER(0x0080001A, get_global_userdata),
        BRIDGE_REGISTER(0x00800030, hle_dispatch),

        /* SLOW EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00, object_next),
        BRIDGE_REGISTER(0x01, chunk_base),
        BRIDGE_REGISTER(0x02, chunk_size),
        BRIDGE_REGISTER(0x03, chunk_max_size),
        BRIDGE_REGISTER(0x05, tick_count),
        BRIDGE_REGISTER(0x0C, imb_range),
        BRIDGE_REGISTER(0x0E, library_lookup),
        BRIDGE_REGISTER(0x11, mutex_wait),
        BRIDGE_REGISTER(0x12, mutex_signal),
        BRIDGE_REGISTER(0x13, process_get_id),
        BRIDGE_REGISTER(0x14, dll_filename),
        BRIDGE_REGISTER(0x15, process_resume),
        BRIDGE_REGISTER(0x16, process_filename),
        BRIDGE_REGISTER(0x17, process_command_line),
        BRIDGE_REGISTER(0x18, process_exit_type),
        BRIDGE_REGISTER(0x1C, process_set_priority),
        BRIDGE_REGISTER(0x1E, process_set_flags),
        BRIDGE_REGISTER(0x1F, semaphore_wait),
        BRIDGE_REGISTER(0x20, semaphore_signal),
        BRIDGE_REGISTER(0x22, server_receive),
        BRIDGE_REGISTER(0x23, server_cancel),
        BRIDGE_REGISTER(0x24, set_session_ptr),
        BRIDGE_REGISTER(0x25, session_send),
        BRIDGE_REGISTER(0x26, thread_id),
        BRIDGE_REGISTER(0x27, session_share),
        BRIDGE_REGISTER(0x28, thread_resume),
        BRIDGE_REGISTER(0x29, thread_suspend),
        BRIDGE_REGISTER(0x2B, thread_set_priority),
        BRIDGE_REGISTER(0x2F, thread_set_flags),
        BRIDGE_REGISTER(0x31, thread_exit_type),
        BRIDGE_REGISTER(0x35, timer_cancel),
        BRIDGE_REGISTER(0x36, timer_after),
        BRIDGE_REGISTER(0x39, change_notifier_logon),
        BRIDGE_REGISTER(0x3A, change_notifier_logoff),
        BRIDGE_REGISTER(0x3B, request_signal),
        BRIDGE_REGISTER(0x3C, handle_name),
        BRIDGE_REGISTER(0x40, after),
        BRIDGE_REGISTER(0x42, message_complete),
        BRIDGE_REGISTER(0x44, time_now),

        BRIDGE_REGISTER(0x4B, add_event),
        BRIDGE_REGISTER(0x4C, session_send_sync),
        BRIDGE_REGISTER(0x4D, dll_tls),
        BRIDGE_REGISTER(0x4E, hal_function),
        BRIDGE_REGISTER(0x51, process_command_line_length),
        BRIDGE_REGISTER(0x54, clear_inactivity_time),
        BRIDGE_REGISTER(0x55, debug_print),
        BRIDGE_REGISTER(0x59, exception_handler),
        BRIDGE_REGISTER(0x5A, set_exception_handler),
        BRIDGE_REGISTER(0x5D, is_exception_handled),
        BRIDGE_REGISTER(0x63, process_type),
        BRIDGE_REGISTER(0x67, thread_create),
        BRIDGE_REGISTER(0x69, handle_close),
        BRIDGE_REGISTER(0x6A, chunk_new),
        BRIDGE_REGISTER(0x6B, chunk_adjust),
        BRIDGE_REGISTER(0x6C, handle_open_object),
        BRIDGE_REGISTER(0x6D, handle_duplicate),
        BRIDGE_REGISTER(0x6E, mutex_create),
        BRIDGE_REGISTER(0x6F, semaphore_create),
        BRIDGE_REGISTER(0x72, thread_kill),
        BRIDGE_REGISTER(0x73, thread_logon),
        BRIDGE_REGISTER(0x74, thread_logon_cancel),
        BRIDGE_REGISTER(0x75, dll_set_tls),
        BRIDGE_REGISTER(0x76, dll_free_tls),
        BRIDGE_REGISTER(0x77, thread_rename),
        BRIDGE_REGISTER(0x78, process_rename),
        BRIDGE_REGISTER(0x7A, process_logon),
        BRIDGE_REGISTER(0x7D, server_create),
        BRIDGE_REGISTER(0x7E, session_create),
        BRIDGE_REGISTER(0x83, timer_create),
        BRIDGE_REGISTER(0x85, after), // Actually AfterHighRes
        BRIDGE_REGISTER(0x86, change_notifier_create),
        BRIDGE_REGISTER(0x9B, wait_dll_lock),
        BRIDGE_REGISTER(0x9D, library_attach),
        BRIDGE_REGISTER(0x9E, library_attached),
        BRIDGE_REGISTER(0x9F, static_call_list),
        BRIDGE_REGISTER(0xA2, last_thread_handle),
        BRIDGE_REGISTER(0xA4, process_rendezvous),
        BRIDGE_REGISTER(0xA5, message_get_des_length),
        BRIDGE_REGISTER(0xA6, message_get_des_max_length),
        BRIDGE_REGISTER(0xA7, message_ipc_copy),
        BRIDGE_REGISTER(0xA8, message_client),
        BRIDGE_REGISTER(0xAD, process_security_info),
        BRIDGE_REGISTER(0xAE, thread_security_info),
        BRIDGE_REGISTER(0xAF, message_security_info),
        BRIDGE_REGISTER(0xB8, message_queue_notify_data_available),
        BRIDGE_REGISTER(0xB4, message_queue_send),
        BRIDGE_REGISTER(0xB5, message_queue_receive),
        BRIDGE_REGISTER(0xB9, message_queue_cancel_notify_available),
        BRIDGE_REGISTER(0xBB, property_define),
        BRIDGE_REGISTER(0xBD, property_attach),
        BRIDGE_REGISTER(0xBE, property_subscribe),
        BRIDGE_REGISTER(0xBF, property_cancel),
        BRIDGE_REGISTER(0xC0, property_get_int),
        BRIDGE_REGISTER(0xC1, property_get_bin),
        BRIDGE_REGISTER(0xC4, property_find_get_int),
        BRIDGE_REGISTER(0xC5, property_find_get_bin),
        BRIDGE_REGISTER(0xC6, property_find_set_int),
        BRIDGE_REGISTER(0xD0, process_get_data_parameter),
        BRIDGE_REGISTER(0xD1, process_data_parameter_length),
        BRIDGE_REGISTER(0xDA, plat_sec_diagnostic),
        BRIDGE_REGISTER(0xDB, exception_descriptor),
        BRIDGE_REGISTER(0xDC, thread_request_signal),
        BRIDGE_REGISTER(0xDE, leave_start),
        BRIDGE_REGISTER(0xDF, leave_end),
        BRIDGE_REGISTER(0xE7, btrace_out)
    };
}
