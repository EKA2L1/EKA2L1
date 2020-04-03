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

#include <common/configure.h>
#include <epoc/hal.h>
#include <epoc/svc.h>
#include <epoc/dispatch/dispatcher.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>

#include <epoc/loader/rom.h>

#include <manager/manager.h>
#include <manager/config.h>

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
    static security_policy server_exclamination_point_name_policy({ cap_prot_serv });

    /* TODO:                                       
     * 1. (bentokun) Implement global user data. Global user data should be allocated in global memory region.
    */

    /********************************/
    /*    GET/SET EXECUTIVE CALLS   */
    /*                              */
    /* Fast executive call, use for */
    /* get/set local data.          */
    /*                              */
    /********************************/

    /*! \brief Get the current heap allocator */
    BRIDGE_FUNC(eka2l1::ptr<void>, Heap) {
        auto local_data = current_local_data(sys);

        if (local_data.heap.ptr_address() == 0) {
            LOG_WARN("Allocator is not available.");
        }

        return local_data.heap;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, HeapSwitch, eka2l1::ptr<void> aNewHeap) {
        auto &local_data = current_local_data(sys);
        decltype(aNewHeap) old_heap = local_data.heap;
        local_data.heap = aNewHeap;

        return old_heap;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, TrapHandler) {
        auto local_data = current_local_data(sys);
        return local_data.trap_handler;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, SetTrapHandler, eka2l1::ptr<void> aNewHandler) {
        auto &local_data = current_local_data(sys);
        decltype(aNewHandler) old_handler = local_data.trap_handler;
        local_data.trap_handler = aNewHandler;

        return local_data.trap_handler;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, ActiveScheduler) {
        auto local_data = current_local_data(sys);
        return local_data.scheduler;
    }

    BRIDGE_FUNC(void, SetActiveScheduler, eka2l1::ptr<void> aNewScheduler) {
        auto &local_data = current_local_data(sys);
        local_data.scheduler = aNewScheduler;
    }

    BRIDGE_FUNC(void, After, std::int32_t aMicroSecs, eka2l1::ptr<epoc::request_status> aStatus) {
        sys->get_kernel_system()->crr_thread()->after(aStatus, aMicroSecs);
    }

    /****************************/
    /* PROCESS */
    /***************************/

    BRIDGE_FUNC(std::int32_t, ProcessExitType, std::int32_t aHandle) {
        eka2l1::memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(aHandle);

        if (!pr_real) {
            LOG_ERROR("ProcessExitType: Invalid process");
            return 0;
        }

        return static_cast<std::int32_t>(pr_real->get_exit_type());
    }

    BRIDGE_FUNC(void, ProcessRendezvous, std::int32_t aRendezvousCode) {
        sys->get_kernel_system()->crr_process()->rendezvous(aRendezvousCode);
    }

    BRIDGE_FUNC(void, ProcessFilename, std::int32_t aProcessHandle, eka2l1::ptr<des8> aDes) {
        eka2l1::memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(aProcessHandle);
        process_ptr cr_process = kern->crr_process();

        if (!pr_real) {
            LOG_ERROR("SvcProcessFileName: Invalid process");
            return;
        }

        epoc::des8 *des = aDes.get(mem);
        const std::u16string full_path_u16 = pr_real->get_exe_path();

        // Why???...
        const std::string full_path = common::ucs2_to_utf8(full_path_u16);

        des->assign(cr_process, full_path);
    }

    BRIDGE_FUNC(std::int32_t, ProcessGetMemoryInfo, std::int32_t aHandle, eka2l1::ptr<kernel::memory_info> aInfo) {
        kernel::memory_info *info = aInfo.get(sys->get_memory_system());
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(aHandle);

        if (!pr_real) {
            return epoc::error_bad_handle;
        }

        pr_real->get_memory_info(*info);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, ProcessGetId, std::int32_t aHandle) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(aHandle);

        if (!pr_real) {
            LOG_ERROR("ProcessGetId: Invalid process");
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(pr_real->unique_id());
    }

    BRIDGE_FUNC(void, ProcessType, address pr, eka2l1::ptr<epoc::uid_type> uid_type) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real = kern->get<kernel::process>(pr);

        if (!pr_real) {
            LOG_ERROR("SvcProcessType: Invalid process");
            return;
        }

        epoc::uid_type *type = uid_type.get(mem);
        auto tup = pr_real->get_uid_type();

        type->uid1 = std::get<0>(tup);
        type->uid2 = std::get<1>(tup);
        type->uid3 = std::get<2>(tup);
    }

    BRIDGE_FUNC(std::int32_t, ProcessDataParameterLength, std::int32_t aSlot) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::process *crr_process = kern->crr_process();

        if (aSlot >= 16 || aSlot < 0) {
            LOG_ERROR("Invalid slot (slot: {} >= 16 or < 0)", aSlot);
            return epoc::error_argument;
        }

        auto slot = crr_process->get_arg_slot(aSlot);

        if (!slot || !slot->used) {
            LOG_ERROR("Getting descriptor length of unused slot: {}", aSlot);
            return epoc::error_not_found;
        }

        return static_cast<std::int32_t>(slot->data.size());
    }

    BRIDGE_FUNC(std::int32_t, ProcessGetDataParameter, std::int32_t aSlot, eka2l1::ptr<std::uint8_t> aData, std::int32_t aLength) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::process *pr = kern->crr_process();

        if (aSlot >= 16 || aSlot < 0) {
            LOG_ERROR("Invalid slot (slot: {} >= 16 or < 0)", aSlot);
            return epoc::error_argument;
        }

        auto slot = *pr->get_arg_slot(aSlot);

        if (!slot.used) {
            LOG_ERROR("Parameter slot unused, error: {}", aSlot);
            return epoc::error_not_found;
        }

        if (aLength < slot.data.size()) {
            LOG_ERROR("Given length is not large enough to slot length ({} vs {})",
                aLength, slot.data.size());
            return epoc::error_no_memory;
        }

        std::uint8_t *data = aData.get(sys->get_memory_system());
        std::copy(slot.data.begin(), slot.data.end(), data);

        std::size_t written_size = slot.data.size();
        pr->mark_slot_free(static_cast<std::uint8_t>(aSlot));

        return static_cast<std::int32_t>(written_size);
    }

    BRIDGE_FUNC(std::int32_t, ProcessSetDataParameter, std::int32_t aHandle, std::int32_t aSlot, eka2l1::ptr<std::uint8_t> aData, std::int32_t aDataSize) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(aHandle);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        if (aSlot < 0 || aSlot >= 16) {
            LOG_ERROR("Invalid parameter slot: {}, slot number must be in range of 0-15", aSlot);
            return epoc::error_argument;
        }

        auto slot = *pr->get_arg_slot(aSlot);

        if (slot.used) {
            LOG_ERROR("Can't set parameter of an used slot: {}", aSlot);
            return epoc::error_in_use;
        }

        pr->set_arg_slot(aSlot, aData.get(sys->get_memory_system()), aDataSize);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, ProcessCommandLineLength, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(aHandle);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(pr->get_cmd_args().length());
    }

    BRIDGE_FUNC(void, ProcessCommandLine, std::int32_t aHandle, eka2l1::ptr<epoc::des8> aData) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(aHandle);

        if (!pr) {
            LOG_WARN("Process not found with handle: 0x{:x}", aHandle);
            return;
        }

        epoc::des8 *data = aData.get(sys->get_memory_system());

        if (!data) {
            return;
        }

        kernel::process *crr_process = kern->crr_process();

        std::u16string cmdline = pr->get_cmd_args();
        char *data_ptr = data->get_pointer(crr_process);

        memcpy(data_ptr, cmdline.data(), cmdline.length() << 1);
        data->set_length(crr_process, static_cast<std::uint32_t>(cmdline.length() << 1));
    }

    BRIDGE_FUNC(void, ProcessSetFlags, std::int32_t aHandle, std::uint32_t aClearMask, std::uint32_t aSetMask) {
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr = kern->get<kernel::process>(aHandle);

        uint32_t org_flags = pr->get_flags();
        uint32_t new_flags = ((org_flags & ~aClearMask) | aSetMask);
        new_flags = (new_flags ^ org_flags);

        pr->set_flags(org_flags ^ new_flags);
    }

    BRIDGE_FUNC(std::int32_t, ProcessSetPriority, std::int32_t aHandle, std::int32_t aProcessPriority) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(aHandle);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        pr->set_priority(static_cast<eka2l1::kernel::process_priority>(aProcessPriority));

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, ProcessRename, std::int32_t aHandle, eka2l1::ptr<des8> aNewName) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get<kernel::process>(aHandle);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        const std::string new_name = aNewName.get(sys->get_memory_system())->to_std_string(kern->crr_process());

        pr->rename(new_name);

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, ProcessResume, std::int32_t aHandle) {
        process_ptr pr = sys->get_kernel_system()->get<kernel::process>(aHandle);

        if (!pr) {
            return;
        }

        pr->run();
    }

    BRIDGE_FUNC(void, ProcessLogon, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestSts, bool aRendezvous) {
        LOG_TRACE("Logon requested from thread {}", sys->get_kernel_system()->crr_thread()->name());

        process_ptr pr = sys->get_kernel_system()->get<kernel::process>(aHandle);

        if (!pr) {
            return;
        }

        pr->logon(aRequestSts, aRendezvous);
    }

    BRIDGE_FUNC(std::int32_t, ProcessLogonCancel, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestSts, bool aRendezvous) {
        process_ptr pr = sys->get_kernel_system()->get<kernel::process>(aHandle);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        bool logon_cancel_success = pr->logon_cancel(aRequestSts, aRendezvous);

        if (logon_cancel_success) {
            return epoc::error_none;
        }

        return epoc::error_general;
    }

    /********************/
    /* TLS */
    /*******************/

    BRIDGE_FUNC(eka2l1::ptr<void>, DllTls, std::int32_t aHandle, std::int32_t aDllUid) {
        eka2l1::kernel::thread_local_data dat = current_local_data(sys);
        kernel::thread *thr = sys->get_kernel_system()->crr_thread();

        for (const auto &tls : dat.tls_slots) {
            if (tls.handle == aHandle) {
                return tls.pointer;
            }
        }

        LOG_WARN("TLS for 0x{:x}, thread {} return 0, may results unexpected crash", static_cast<std::uint32_t>(aHandle),
            thr->name());

        return eka2l1::ptr<void>(0);
    }

    BRIDGE_FUNC(std::int32_t, DllSetTls, std::int32_t aHandle, std::int32_t aDllUid, eka2l1::ptr<void> aPtr) {
        eka2l1::kernel::tls_slot *slot = get_tls_slot(sys, aHandle);

        if (!slot) {
            return epoc::error_no_memory;
        }

        slot->pointer = aPtr;

        kernel::thread *thr = sys->get_kernel_system()->crr_thread();

        LOG_TRACE("TLS set for 0x{:x}, ptr: 0x{:x}, thread {}", static_cast<std::uint32_t>(aHandle), aPtr.ptr_address(),
            thr->name());

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, DllFreeTLS, std::int32_t iHandle) {
        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        thr->close_tls_slot(*thr->get_tls_slot(iHandle, iHandle));

        LOG_TRACE("TLS slot closed for 0x{:x}, thread {}", static_cast<std::uint32_t>(iHandle), thr->name());
    }

    BRIDGE_FUNC(void, DllFileName, std::int32_t aEntryAddress, eka2l1::ptr<epoc::des8> aFullPathPtr) {
        std::optional<std::u16string> dll_full_path = get_dll_full_path(sys, aEntryAddress);

        if (!dll_full_path) {
            LOG_WARN("Unable to find DLL name for address: 0x{:x}", aEntryAddress);
            return;
        }

        std::string path_utf8 = common::ucs2_to_utf8(*dll_full_path);
        LOG_TRACE("Find DLL for address 0x{:x} with name: {}", static_cast<std::uint32_t>(aEntryAddress),
            path_utf8);

        aFullPathPtr.get(sys->get_memory_system())->assign(sys->get_kernel_system()->crr_process(), path_utf8);
    }

    /***********************************/
    /* LOCALE */
    /**********************************/

    /*
    * Warning: It's not possible to set the UTC time and offset in the emulator at the moment.
    */
   
    BRIDGE_FUNC(std::int32_t, UTCOffset) {
        // TODO: Users and apps can set this
        return common::get_current_utc_offset();
    }

    enum : uint64_t {
        microsecs_per_sec = 1000000,
        ad_epoc_dist_microsecs = 62167132800 * microsecs_per_sec
    };

    BRIDGE_FUNC(std::int32_t, TimeNow, eka2l1::ptr<std::uint64_t> aTime, eka2l1::ptr<std::int32_t> aUTCOffset) {
        std::uint64_t *time = aTime.get(sys->get_memory_system());
        std::int32_t *offset = aUTCOffset.get(sys->get_memory_system());

        // The time is since EPOC, we need to convert it to first of AD
        *time = sys->get_kernel_system()->home_time();
        *offset = common::get_current_utc_offset();

        return epoc::error_none;
    }

    /********************************************/
    /* IPC */
    /*******************************************/

    BRIDGE_FUNC(void, SetSessionPtr, std::int32_t aMsgHandle, std::uint32_t aSessionAddress) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        ipc_msg_ptr msg = kern->get_msg(aMsgHandle);

        if (!msg) {
            return;
        }

        msg->msg_session->set_cookie_address(aSessionAddress);
    }

    BRIDGE_FUNC(std::int32_t, MessageComplete, std::int32_t aMsgHandle, std::int32_t aVal) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        ipc_msg_ptr msg = kern->get_msg(aMsgHandle);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        if (msg->request_sts) {
            *(msg->request_sts.get(msg->own_thr->owning_process())) = aVal;
            msg->own_thr->signal_request();
        }

        LOG_TRACE("Message completed with code: {}, thread to signal: {}", aVal, msg->own_thr->name());

#ifdef ENABLE_SCRIPTING
        // Invoke hook
        sys->get_manager_system()->get_script_manager()->call_ipc_complete(msg->msg_session->get_server()->name(),
            msg->function, msg.get());
#endif

        // Free the message
        msg->free = true;

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, MessageKill, std::int32_t aHandle, TExitType aExitType, std::int32_t aReason, eka2l1::ptr<desc8> aCage) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        std::string exit_cage = aCage.get(mem)->to_std_string(kern->crr_process());
        std::optional<std::string> exit_description;

        ipc_msg_ptr msg = kern->get_msg(aHandle);

        if (!msg || !msg->own_thr) {
            return epoc::error_bad_handle;
        }

        std::string thread_name = msg->own_thr->name();

        if (is_panic_category_action_default(exit_cage)) {
            exit_description = get_panic_description(exit_cage, aReason);

            switch (aExitType) {
            case TExitType::panic:
                LOG_TRACE("Thread {} paniced by message with cagetory: {} and exit code: {} {}", thread_name, exit_cage, aReason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            case TExitType::kill:
                LOG_TRACE("Thread {} forcefully killed by message with cagetory: {} and exit code: {}", thread_name, exit_cage, aReason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            case TExitType::terminate:
            case TExitType::pending:
                LOG_TRACE("Thread {} terminated peacefully by message with cagetory: {} and exit code: {}", thread_name, exit_cage, aReason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            default:
                return epoc::error_argument;
            }
        }

#ifdef ENABLE_SCRIPTING
        sys->get_manager_system()->get_script_manager()->call_panics(exit_cage, aReason);
#endif

        kern->get_thread_scheduler()->stop(msg->own_thr);
        kern->prepare_reschedule();

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, MessageGetDesLength, std::int32_t aHandle, std::int32_t aParam) {
        if (aParam < 0) {
            return epoc::error_argument;
        }

        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        ipc_msg_ptr msg = kern->get_msg(aHandle);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        if ((int)msg->args.get_arg_type(aParam) & (int)ipc_arg_type::flag_des) {
            epoc::desc_base *base = eka2l1::ptr<epoc::desc_base>(msg->args.args[aParam]).get(msg->own_thr->owning_process());

            return base->get_length();
        }

        return epoc::error_bad_descriptor;
    }

    BRIDGE_FUNC(std::int32_t, MessageGetDesMaxLength, std::int32_t aHandle, std::int32_t aParam) {
        if (aParam < 0) {
            return epoc::error_argument;
        }

        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        ipc_msg_ptr msg = kern->get_msg(aHandle);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        const ipc_arg_type type = msg->args.get_arg_type(aParam);

        if ((int)type & (int)ipc_arg_type::flag_des) {
            kernel::process *own_pr = msg->own_thr->owning_process();
            return eka2l1::ptr<epoc::des8>(msg->args.args[aParam]).get(own_pr)->get_max_length(own_pr);
        }

        return epoc::error_bad_descriptor;
    }

    struct TIpcCopyInfo {
        eka2l1::ptr<std::uint8_t> iTargetPtr;
        int iTargetLength;
        int iFlags;
    };

    static_assert(sizeof(TIpcCopyInfo) == 12, "Size of IPCCopy struct is 12");

    const std::int32_t KChunkShiftBy0 = 0;
    const std::int32_t KChunkShiftBy1 = (std::int32_t)0x80000000;
    const std::int32_t KIpcDirRead = 0;
    const std::int32_t KIpcDirWrite = 0x10000000;

    // In source code, the usage of this SVC call is like this:
    // - Read: Success returns the length readed, and set the target receive descriptor to that length.
    // - Write: returns epoc::error_none if success
    // This call can be much optimized more, but will results in ugly code.
    // TODO (pent0): Optimize this, since this is call frequently
    BRIDGE_FUNC(std::int32_t, MessageIpcCopy, std::int32_t aHandle, std::int32_t aParam, eka2l1::ptr<TIpcCopyInfo> aInfo, std::int32_t aStartOffset) {
        if (!aInfo || aParam < 0) {
            return epoc::error_argument;
        }

        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        process_ptr crr_process = kern->crr_process();

        TIpcCopyInfo *info = aInfo.get(crr_process);
        ipc_msg_ptr msg = kern->get_msg(aHandle);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        bool des8 = true;
        if (info->iFlags & KChunkShiftBy1) {
            des8 = false;
        }

        bool read = true;
        if (info->iFlags & KIpcDirWrite) {
            read = false;
        }

        if (read) {
            service::ipc_context context(false);
            context.sys = sys;
            context.msg = msg;

            if (des8) {
                const auto arg_request = context.get_arg<std::string>(aParam);

                if (!arg_request) {
                    return epoc::error_bad_descriptor;
                }

                std::int32_t lengthToRead = common::min(
                    static_cast<std::int32_t>(arg_request->length()) - aStartOffset, info->iTargetLength);

                memcpy(info->iTargetPtr.get(crr_process), arg_request->data() + aStartOffset, lengthToRead);
                return lengthToRead;
            }

            const auto arg_request = context.get_arg<std::u16string>(aParam);

            if (!arg_request) {
                return epoc::error_bad_descriptor;
            }

            std::int32_t lengthToRead = common::min(
                static_cast<std::int32_t>(arg_request->length()) - aStartOffset, info->iTargetLength);

            memcpy(info->iTargetPtr.get(crr_process), reinterpret_cast<const std::uint8_t *>(arg_request->data()) + aStartOffset * 2,
                lengthToRead * 2);

            return lengthToRead;
        }

        service::ipc_context context(false);
        context.sys = sys;
        context.msg = msg;

        std::string content;
        
        // We must keep the other part behind the offset
        if (des8) {
            content = std::move(*context.get_arg<std::string>(aParam));
        } else {
            std::u16string temp_content = *context.get_arg<std::u16string>(aParam);
            content.resize(temp_content.length() * 2);
            memcpy(&content[0], &temp_content[0], content.length());
        }

        std::uint32_t minimum_size = aStartOffset + info->iTargetLength;
        des8 ? 0 : minimum_size *= 2;

        if (content.length() < minimum_size) {
            content.resize(minimum_size);
        }

        memcpy(&content[des8 ? aStartOffset : aStartOffset * 2], info->iTargetPtr.get(mem),
            des8 ? info->iTargetLength : info->iTargetLength * 2);

        int error_code = 0;

        bool result = context.write_arg_pkg(aParam,
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

    static void query_security_info(kernel::process *process, epoc::security_info *info) {
        assert(process);

        *info = std::move(process->get_sec_info());
    }

    BRIDGE_FUNC(void, ProcessSecurityInfo, std::int32_t aProcessHandle, eka2l1::ptr<epoc::security_info> aSecInfo) {
        epoc::security_info *sec_info = aSecInfo.get(sys->get_memory_system());
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr = kern->get<kernel::process>(aProcessHandle);
        query_security_info(&(*pr), sec_info);
    }

    BRIDGE_FUNC(void, ThreadSecurityInfo, std::int32_t aThreadHandle, eka2l1::ptr<epoc::security_info> aSecInfo) {
        epoc::security_info *sec_info = aSecInfo.get(sys->get_memory_system());
        kernel_system *kern = sys->get_kernel_system();

        thread_ptr thr = kern->get<kernel::thread>(aThreadHandle);

        if (!thr) {
            LOG_ERROR("Thread handle invalid 0x{:x}", aThreadHandle);
            return;
        }

        query_security_info(thr->owning_process(), sec_info);
    }

    BRIDGE_FUNC(void, MessageSecurityInfo, std::int32_t aMessageHandle, eka2l1::ptr<epoc::security_info> aSecInfo) {
        epoc::security_info *sec_info = aSecInfo.get(sys->get_memory_system());
        kernel_system *kern = sys->get_kernel_system();

        eka2l1::ipc_msg_ptr msg = kern->get_msg(aMessageHandle);

        if (!msg) {
            LOG_ERROR("Thread handle invalid 0x{:x}", aMessageHandle);
            return;
        }

        query_security_info(msg->own_thr->owning_process(), sec_info);
    }

    BRIDGE_FUNC(std::int32_t, ServerCreate, eka2l1::ptr<desc8> aServerName, std::int32_t aMode) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::process *crr_pr = std::move(kern->crr_process());

        std::string server_name = aServerName.get(sys->get_memory_system())
                                      ->to_std_string(crr_pr);

        // Exclamination point at the beginning of server name requires ProtServ
        if (!server_name.empty() && server_name[0] == '!') {
            if (!crr_pr->satisfy(server_exclamination_point_name_policy)) {
                LOG_ERROR("Process {} try to create a server with exclamination point at the beginning of name ({}),"
                          " but doesn't have ProtServ", crr_pr->name(), server_name);

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

    BRIDGE_FUNC(void, ServerReceive, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestStatus, eka2l1::ptr<void> aDataPtr) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        server_ptr server = kern->get<service::server>(aHandle);

        if (!server) {
            return;
        }

        LOG_TRACE("Receive requested from {}", server->name());

        server->receive_async_lle(aRequestStatus, aDataPtr.cast<service::message2>());
    }

    BRIDGE_FUNC(void, ServerCancel, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        server_ptr server = kern->get<service::server>(aHandle);

        if (!server) {
            return;
        }

        server->cancel_async_lle();
    }

    static std::int32_t do_create_session_from_server(system *sys, server_ptr server, std::int32_t msg_slot_count, eka2l1::ptr<void> sec, std::int32_t mode) {
        kernel_system *kern = sys->get_kernel_system();
        const kernel::handle handle = kern->create_and_add<service::session>(
                                              kernel::owner_type::process, server, msg_slot_count)
                                          .first;

        if (handle == INVALID_HANDLE) {
            return epoc::error_general;
        }

        LOG_TRACE("New session connected to {} with handle {}", server->name(), handle);

        return handle;
    }

    BRIDGE_FUNC(std::int32_t, SessionCreate, eka2l1::ptr<desc8> aServerName, std::int32_t aMsgSlot, eka2l1::ptr<void> aSec, std::int32_t aMode) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr = kern->crr_process();

        std::string server_name = aServerName.get(pr)->to_std_string(pr);
        server_ptr server = kern->get_by_name<service::server>(server_name);

        if (!server) {
            LOG_TRACE("Create session to unexist server: {}", server_name);
            return epoc::error_not_found;
        }

        return do_create_session_from_server(sys, server, aMsgSlot, aSec, aMode);
    }

    BRIDGE_FUNC(std::int32_t, SessionCreateFromHandle, std::uint32_t aServerHandle, std::int32_t aMsgSlot, eka2l1::ptr<void> aSec, std::int32_t aMode) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr = kern->crr_process();
        server_ptr server = kern->get<service::server>(aServerHandle);

        if (!server) {
            LOG_TRACE("Create session to unexist server handle: {}", aServerHandle);
            return epoc::error_not_found;
        }

        return do_create_session_from_server(sys, server, aMsgSlot, aSec, aMode);
    }

    BRIDGE_FUNC(std::int32_t, SessionShare, eka2l1::ptr<std::int32_t> aHandle, std::int32_t aShare) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        std::int32_t *handle = aHandle.get(mem);

        session_ptr ss = kern->get<service::session>(*handle);

        if (!ss) {
            return epoc::error_bad_handle;
        }

        if (aShare == 2) {
            // Explicit attach: other process uses IPC can open this handle, so do threads! :D
            ss->set_access_type(kernel::access_type::global_access);
        } else {
            ss->set_access_type(kernel::access_type::local_access);
        }

        int old_handle = *handle;

        // Weird behavior, suddenly it wants to mirror handle then close the old one
        // Clean its identity :D
        *handle = kern->mirror(kern->crr_thread(), *handle, kernel::owner_type::process);
        kern->close(old_handle);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, SessionSendSync, std::int32_t aHandle, std::int32_t aOrd, eka2l1::ptr<void> aIpcArgs,
        eka2l1::ptr<epoc::request_status> aStatus) {
        // LOG_TRACE("Send using handle: {}", (aHandle & 0x8000) ? (aHandle & ~0x8000) : (aHandle));

        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        // Dispatch the header
        ipc_arg arg;
        std::int32_t *arg_header = aIpcArgs.cast<std::int32_t>().get(mem);

        if (aIpcArgs) {
            for (uint8_t i = 0; i < 4; i++) {
                arg.args[i] = *arg_header++;
            }

            arg.flag = *arg_header & (((1 << 12) - 1) | (int)ipc_arg_pin::pin_mask);
        }

        session_ptr ss = kern->get<service::session>(aHandle);

        if (!ss) {
            return epoc::error_bad_handle;
        }

        if (!aStatus) {
            LOG_TRACE("Sending a blind sync message");
        }

        if (sys->get_config()->log_ipc) {
            LOG_TRACE("Sending {} sync to {}", aOrd, ss->get_server()->name());
        }

#ifdef ENABLE_SCRIPTING
        sys->get_manager_system()->get_script_manager()->call_ipc_send(ss->get_server()->name(),
            aOrd, arg.args[0], arg.args[1], arg.args[2], arg.args[3], arg.flag,
            sys->get_kernel_system()->crr_thread());
#endif

        const int result = ss->send_receive_sync(aOrd, arg, aStatus);

        if (ss->get_server()->is_hle()) {
            // Process it right away.
            ss->get_server()->process_accepted_msg();
        }

        return result;
    }

    BRIDGE_FUNC(std::int32_t, SessionSend, std::int32_t aHandle, std::int32_t aOrd, eka2l1::ptr<void> aIpcArgs,
        eka2l1::ptr<epoc::request_status> aStatus) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        // Dispatch the header
        ipc_arg arg;
        std::int32_t *arg_header = aIpcArgs.cast<std::int32_t>().get(mem);

        if (aIpcArgs) {
            for (uint8_t i = 0; i < 4; i++) {
                arg.args[i] = *arg_header++;
            }

            arg.flag = *arg_header & (((1 << 12) - 1) | (int)ipc_arg_pin::pin_mask);
        }

        session_ptr ss = kern->get<service::session>(aHandle);

        if (!ss) {
            return epoc::error_bad_handle;
        }

        if (!aStatus) {
            LOG_TRACE("Sending a blind async message");
        }

        if (sys->get_config()->log_ipc) {
            LOG_TRACE("Sending {} to {}", aOrd, ss->get_server()->name());
        }

#ifdef ENABLE_SCRIPTING
        sys->get_manager_system()->get_script_manager()->call_ipc_send(ss->get_server()->name(),
            aOrd, arg.args[0], arg.args[1], arg.args[2], arg.args[3], arg.flag,
            sys->get_kernel_system()->crr_thread());
#endif

        const int result = ss->send_receive(aOrd, arg, aStatus);
        
        if (ss->get_server()->is_hle()) {
            // Process it right away.
            ss->get_server()->process_accepted_msg();
        }

        return result;
    }

    /**********************************/
    /* TRAP/LEAVE */
    /*********************************/

    BRIDGE_FUNC(eka2l1::ptr<void>, LeaveStart) {
        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        LOG_CRITICAL("Leave started! Guess leave code: {}", static_cast<std::int32_t>(sys->get_cpu()->get_reg(0)));

        thr->increase_leave_depth();

        return current_local_data(sys).trap_handler;
    }

    BRIDGE_FUNC(void, LeaveEnd) {
        kernel::thread *thr = sys->get_kernel_system()->crr_thread();
        thr->decrease_leave_depth();

        if (thr->is_invalid_leave()) {
            LOG_CRITICAL("Invalid leave, leave depth is negative!");
        }

        LOG_TRACE("Leave trapped by trap handler.");
    }

    BRIDGE_FUNC(std::int32_t, DebugMask) {
        return 0;
    }

    BRIDGE_FUNC(void, SetDebugMask, std::int32_t aDebugMask) {
        // Doing nothing here
    }

    BRIDGE_FUNC(std::int32_t, DebugMaskIndex, std::int32_t aIdx) {
        return 0;
    }

    BRIDGE_FUNC(std::int32_t, HalFunction, std::int32_t aCagetory, std::int32_t aFunc, eka2l1::ptr<std::int32_t> a1, eka2l1::ptr<std::int32_t> a2) {
        memory_system *mem = sys->get_memory_system();

        int *arg1 = a1.get(mem);
        int *arg2 = a2.get(mem);

        return do_hal(sys, aCagetory, aFunc, arg1, arg2);
    }

    /**********************************/
    /* CHUNK */
    /*********************************/

    BRIDGE_FUNC(std::int32_t, ChunkCreate, epoc::owner_type aOwnerType, eka2l1::ptr<desc8> aName, eka2l1::ptr<epoc::chunk_create> aChunkCreate) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        epoc::chunk_create createInfo = *aChunkCreate.get(mem);
        desc8 *name = aName.get(mem);

        kernel::chunk_type type = kernel::chunk_type::normal;
        kernel::chunk_access access = kernel::chunk_access::local;
        kernel::chunk_attrib att = decltype(att)::none;
        prot perm = prot::read_write;

        // Fetch chunk type
        if (createInfo.att & epoc::chunk_create::disconnected) {
            type = kernel::chunk_type::disconnected;
        } else if (createInfo.att & epoc::chunk_create::double_ended) {
            type = kernel::chunk_type::double_ended;
        }

        // Fetch chunk access
        if (createInfo.att & epoc::chunk_create::global) {
            access = kernel::chunk_access::global;
        } else {
            access = kernel::chunk_access::local;
        }

        if (createInfo.att & epoc::chunk_create::code) {
            perm = prot::read_write_exec;
        }

        if ((access == decltype(access)::global) && ((!name) || (name->get_length() == 0))) {
            att = kernel::chunk_attrib::anonymous;
        }

        const kernel::handle handle = kern->create_and_add<kernel::chunk>(
                                              aOwnerType == epoc::owner_process ? kernel::owner_type::process : kernel::owner_type::thread,
                                              sys->get_memory_system(), kern->crr_process(), name ? name->to_std_string(kern->crr_process()) : "", createInfo.initial_bottom,
                                              createInfo.initial_top, createInfo.max_size, perm, type, access, att)
                                          .first;

        if (handle == INVALID_HANDLE) {
            LOG_TRACE("AAAAAAAAAAAa");
            return epoc::error_no_memory;
        }

        return handle;
    }

    BRIDGE_FUNC(std::int32_t, ChunkMaxSize, std::int32_t aChunkHandle) {
        chunk_ptr chunk = sys->get_kernel_system()->get<kernel::chunk>(aChunkHandle);
        if (!chunk) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(chunk->max_size());
    }

    BRIDGE_FUNC(eka2l1::ptr<std::uint8_t>, ChunkBase, std::int32_t aChunkHandle) {
        chunk_ptr chunk = sys->get_kernel_system()->get<kernel::chunk>(aChunkHandle);
        if (!chunk) {
            return 0;
        }

        return chunk->base();
    }
    
    BRIDGE_FUNC(std::int32_t, ChunkSize, std::int32_t aChunkHandle) {
        chunk_ptr chunk = sys->get_kernel_system()->get<kernel::chunk>(aChunkHandle);
        if (!chunk) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(chunk->committed());
    }

    BRIDGE_FUNC(std::int32_t, ChunkAdjust, std::int32_t aChunkHandle, std::int32_t aType, std::int32_t a1, std::int32_t a2) {
        chunk_ptr chunk = sys->get_kernel_system()->get<kernel::chunk>(aChunkHandle);

        if (!chunk) {
            return epoc::error_bad_handle;
        }

        auto fetch = [aType](chunk_ptr chunk, int a1, int a2) -> bool {
            switch (aType) {
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

    BRIDGE_FUNC(void, IMB_Range, eka2l1::ptr<void> aAddress, std::uint32_t aSize) {
        sys->get_cpu()->imb_range(aAddress.ptr_address(), aSize);
    }

    /********************/
    /* SYNC PRIMITIVES  */
    /********************/

    BRIDGE_FUNC(std::int32_t, SemaphoreCreate, eka2l1::ptr<desc8> aSemaName, std::int32_t aInitCount, epoc::owner_type aOwnerType) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        desc8 *desname = aSemaName.get(mem);
        kernel::owner_type owner = (aOwnerType == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread;

        const kernel::handle sema = kern->create_and_add<kernel::semaphore>(owner, !desname ? "" : desname->to_std_string(kern->crr_process()).c_str(),
                                            aInitCount, !desname ? kernel::access_type::local_access : kernel::access_type::global_access)
                                        .first;

        if (sema == INVALID_HANDLE) {
            return epoc::error_general;
        }

        return sema;
    }

    BRIDGE_FUNC(std::int32_t, SemaphoreWait, std::int32_t aSemaHandle, std::int32_t aTimeout) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        sema_ptr sema = kern->get<kernel::semaphore>(aSemaHandle);

        if (!sema) {
            return epoc::error_bad_handle;
        }

        if (aTimeout) {
            LOG_WARN("Semaphore timeout unimplemented");
        }

        sema->wait();
        return epoc::error_none;
    }

    BRIDGE_FUNC(void, SemaphoreSignal, std::int32_t aSemaHandle) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        sema_ptr sema = kern->get<kernel::semaphore>(aSemaHandle);

        if (!sema) {
            return;
        }

        sema->signal(1);
    }

    BRIDGE_FUNC(void, SemaphoreSignalN, std::int32_t aSemaHandle, std::int32_t aSigCount) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        sema_ptr sema = kern->get<kernel::semaphore>(aSemaHandle);

        if (!sema) {
            return;
        }

        sema->signal(aSigCount);
    }

    BRIDGE_FUNC(std::int32_t, MutexCreate, eka2l1::ptr<desc8> aMutexName, epoc::owner_type aOwnerType) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        desc8 *desname = aMutexName.get(mem);
        kernel::owner_type owner = (aOwnerType == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread;

        const kernel::handle mut = kern->create_and_add<kernel::mutex>(owner, sys->get_timing_system(),
                                           !desname ? "" : desname->to_std_string(kern->crr_process()), false,
                                           !desname ? kernel::access_type::local_access : kernel::access_type::global_access)
                                       .first;

        if (mut == INVALID_HANDLE) {
            return epoc::error_general;
        }

        return mut;
    }

    BRIDGE_FUNC(std::int32_t, MutexWait, std::int32_t aMutexHandle) {
        kernel_system *kern = sys->get_kernel_system();
        mutex_ptr mut = kern->get<kernel::mutex>(aMutexHandle);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return epoc::error_bad_handle;
        }

        mut->wait();
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, MutexWaitVer2, std::int32_t aMutexHandle, std::int32_t aTimeout) {
        kernel_system *kern = sys->get_kernel_system();
        mutex_ptr mut = kern->get<kernel::mutex>(aMutexHandle);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return epoc::error_bad_handle;
        }

        if (aTimeout == 0) {
            mut->wait();
            return epoc::error_none;
        }

        if (aTimeout == -1) {
            // Try lock
            mut->try_wait();
            return epoc::error_none;
        }

        mut->wait_for(aTimeout);
        return epoc::error_none;
    }

    BRIDGE_FUNC(void, MutexSignal, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        mutex_ptr mut = kern->get<kernel::mutex>(aHandle);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return;
        }

        mut->signal(kern->crr_thread());
    }

    BRIDGE_FUNC(void, WaitForAnyRequest) {
        sys->get_kernel_system()->crr_thread()->wait_for_any_request();
    }

    BRIDGE_FUNC(void, RequestSignal, int aSignalCount) {
        sys->get_kernel_system()->crr_thread()->signal_request(aSignalCount);
    }

    /***********************************************/
    /* HANDLE FUNCTIONS   */
    /*                    */
    /* Thread independent */
    /**********************************************/

    BRIDGE_FUNC(std::int32_t, ObjectNext, TObjectType aObjectType, eka2l1::ptr<des8> aName, eka2l1::ptr<epoc::find_handle> aHandleFind) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        epoc::find_handle *handle = aHandleFind.get(mem);
        std::string name = aName.get(mem)->to_std_string(kern->crr_process());

        LOG_TRACE("Finding object name: {}", name);

        std::optional<eka2l1::find_handle> info = kern->find_object(name, handle->handle,
            static_cast<kernel::object_type>(aObjectType), true);

        if (!info) {
            return epoc::error_not_found;
        }

        handle->handle = info->index;
        handle->obj_id_low = static_cast<uint32_t>(info->object_id);

        // We are never gonna reached the high part
        handle->obj_id_high = 0;

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, HandleClose, std::int32_t aHandle) {
        if (aHandle & 0x8000) {
            return epoc::error_general;
        }

        int res = sys->get_kernel_system()->close(aHandle);

        return res;
    }

    BRIDGE_FUNC(std::int32_t, HandleDuplicate, std::int32_t aThreadHandle, epoc::owner_type aOwnerType, std::int32_t aDupHandle) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        uint32_t res = kern->mirror(&(*kern->get<kernel::thread>(aThreadHandle)), aDupHandle,
            (aOwnerType == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread);

        return res;
    }

    BRIDGE_FUNC(std::int32_t, HandleOpenObject, TObjectType aObjectType, eka2l1::ptr<epoc::desc8> aName, std::int32_t aOwnerType) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        std::string obj_name = aName.get(mem)->to_std_string(kern->crr_process());

        // What a waste if we find the ID already but not mirror it
        kernel_obj_ptr obj = kern->get_by_name_and_type<kernel::kernel_obj>(obj_name,
            static_cast<kernel::object_type>(aObjectType));

        if (!obj) {
            LOG_ERROR("Can't open object: {}", obj_name);
            return epoc::error_not_found;
        }

        uint32_t ret_handle = kern->mirror(obj,
            static_cast<eka2l1::kernel::owner_type>(aOwnerType));

        if (ret_handle != INVALID_HANDLE) {
            return ret_handle;
        }

        return epoc::error_general;
    }

    BRIDGE_FUNC(void, HandleName, std::int32_t aHandle, eka2l1::ptr<des8> aName) {
        kernel_system *kern = sys->get_kernel_system();
        kernel_obj_ptr obj = kern->get_kernel_obj_raw(aHandle);

        if (!obj) {
            return;
        }

        des8 *desname = aName.get(sys->get_memory_system());
        desname->assign(kern->crr_process(), obj->name());
    }

    BRIDGE_FUNC(void, HandleFullName, std::int32_t aHandle, eka2l1::ptr<des8> aFullname) {
        kernel_system *kern = sys->get_kernel_system();
        kernel_obj_ptr obj = kern->get_kernel_obj_raw(aHandle);

        if (!obj) {
            return;
        }

        des8 *desname = aFullname.get(sys->get_memory_system());

        std::string full_name;
        obj->full_name(full_name);

        desname->assign(kern->crr_process(), full_name);
    }

    /******************************/
    /* CODE SEGMENT */
    /*****************************/

    BRIDGE_FUNC(std::int32_t, StaticCallList, eka2l1::ptr<std::int32_t> aTotal, eka2l1::ptr<std::uint32_t> aList) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::process *pr = kern->crr_process();

        std::uint32_t *list_ptr = aList.get(pr);
        std::int32_t *total = aTotal.get(pr);

        std::vector<uint32_t> list;
        pr->get_codeseg()->queries_call_list(pr, list);

        *total = static_cast<std::int32_t>(list.size());
        memcpy(list_ptr, list.data(), sizeof(std::uint32_t) * *total);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, WaitDllLock) {
        sys->get_kernel_system()->crr_process()->wait_dll_lock();

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, ReleaseDllLock) {
        sys->get_kernel_system()->crr_process()->signal_dll_lock(
            sys->get_kernel_system()->crr_thread());

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, LibraryAttach, std::int32_t aHandle, eka2l1::ptr<std::int32_t> aNumEps, eka2l1::ptr<std::uint32_t> aEpList) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        library_ptr lib = kern->get<kernel::library>(aHandle);

        if (!lib) {
            return epoc::error_bad_handle;
        }

        std::vector<uint32_t> entries = lib->attach(kern->crr_process());

        *aNumEps.get(mem) = static_cast<std::int32_t>(entries.size());

        for (size_t i = 0; i < entries.size(); i++) {
            (aEpList.get(mem))[i] = entries[i];
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, LibraryLookup, std::int32_t aHandle, std::int32_t aOrdinalIndex) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        library_ptr lib = kern->get<kernel::library>(aHandle);

        if (!lib) {
            return 0;
        }

        std::optional<uint32_t> func_addr = lib->get_ordinal_address(kern->crr_process(),
            static_cast<std::uint32_t>(aOrdinalIndex));

        if (!func_addr) {
            return 0;
        }

        return *func_addr;
    }

    BRIDGE_FUNC(std::int32_t, LibraryAttached, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        library_ptr lib = kern->get<kernel::library>(aHandle);

        if (!lib) {
            return epoc::error_bad_handle;
        }

        bool attached_result = lib->attached();

        if (!attached_result) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    /************************/
    /* USER SERVER */
    /***********************/

    BRIDGE_FUNC(std::int32_t, UserSvrRomHeaderAddress) {
        // EKA1
        if ((int)sys->get_kernel_system()->get_epoc_version() <= (int)epocver::epoc6) {
            return 0x50000000;
        }

        // EKA2
        return 0x80000000;
    }

    BRIDGE_FUNC(std::int32_t, UserSvrRomRootDirAddress) {
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

    BRIDGE_FUNC(std::int32_t, ThreadCreate, eka2l1::ptr<desc8> aThreadName, epoc::owner_type aOwnerType, eka2l1::ptr<thread_create_info_expand> aInfo) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        process_ptr pr = kern->crr_process();

        // Get rid of null terminator
        std::string thr_name = aThreadName.get(pr)->to_std_string(pr).c_str();
        thread_create_info_expand *info = aInfo.get(pr);

        if (thr_name.empty()) {
            thr_name = "AnonymousThread";
        }

        const kernel::handle thr_handle = kern->create_and_add<kernel::thread>(static_cast<kernel::owner_type>(aOwnerType),
                                                  mem, kern->get_timing_system(), kern->crr_process(),
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

    BRIDGE_FUNC(std::int32_t, LastThreadHandle) {
        return sys->get_kernel_system()->crr_thread()->last_handle();
    }

    BRIDGE_FUNC(std::int32_t, ThreadID, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(aHandle);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(thr->unique_id());
    }

    BRIDGE_FUNC(std::int32_t, ThreadKill, std::int32_t aHandle, TExitType aExitType, std::int32_t aReason, eka2l1::ptr<desc8> aReasonDes) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        thread_ptr thr = kern->get<kernel::thread>(aHandle);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        std::string exit_cage = "None";
        std::string thread_name = thr->name();

        if (aReasonDes) {
            exit_cage = aReasonDes.get(mem)->to_std_string(kern->crr_process());
        }

        std::optional<std::string> exit_description;

        if (is_panic_category_action_default(exit_cage)) {
            exit_description = get_panic_description(exit_cage, aReason);

            switch (aExitType) {
            case TExitType::panic:
                LOG_TRACE("Thread {} paniced with cagetory: {} and exit code: {} {}", thread_name, exit_cage, aReason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            case TExitType::kill:
                LOG_TRACE("Thread {} forcefully killed with cagetory: {} and exit code: {} {}", thread_name, exit_cage, aReason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            case TExitType::terminate:
            case TExitType::pending:
                LOG_TRACE("Thread {} terminated peacefully with cagetory: {} and exit code: {}", thread_name, exit_cage, aReason,
                    exit_description ? (std::string("(") + *exit_description + ")") : "");
                break;

            default:
                return epoc::error_argument;
            }
        }

        if (thr->owning_process()->decrease_thread_count() == 0) {
            thr->owning_process()->set_exit_type(static_cast<kernel::process_exit_type>(aExitType));
        }

#ifdef ENABLE_SCRIPTING
        sys->get_manager_system()->get_script_manager()->call_panics(exit_cage, aReason);
#endif

        kern->get_thread_scheduler()->stop(&(*thr));
        kern->prepare_reschedule();

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, ThreadRequestSignal, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        thread_ptr thr = kern->get<kernel::thread>(aHandle);

        if (!thr) {
            return;
        }

        thr->signal_request();
    }

    BRIDGE_FUNC(std::int32_t, ThreadRename, std::int32_t aHandle, eka2l1::ptr<desc8> aName) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        thread_ptr thr = kern->get<kernel::thread>(aHandle);
        desc8 *name = aName.get(mem);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        std::string new_name = name->to_std_string(kern->crr_process());

        LOG_TRACE("Thread with last name: {} renamed to {}", thr->name(), new_name);

        thr->rename(new_name);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, ThreadProcess, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(aHandle);

        return kern->mirror(kern->get_by_id<kernel::process>(thr->owning_process()->unique_id()), 
            kernel::owner_type::thread);
    }

    BRIDGE_FUNC(void, ThreadSetPriority, std::int32_t aHandle, std::int32_t aThreadPriority) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(aHandle);

        if (!thr) {
            return;
        }

        thr->set_priority(static_cast<eka2l1::kernel::thread_priority>(aThreadPriority));
    }

    BRIDGE_FUNC(void, ThreadResume, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(aHandle);

        if (!thr) {
            LOG_ERROR("invalid thread handle 0x{:x}", aHandle);
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

    BRIDGE_FUNC(void, ThreadSuspend, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(aHandle);

        if (!thr) {
            LOG_ERROR("invalid thread handle 0x{:x}", aHandle);
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

    // Hide deep in the kernel is me! the context!
    struct arm_context_epoc {
        std::uint32_t regs[16];
        std::uint32_t cpsr;

        // We currently don't support this register
        std::uint32_t dacr;
    };

    BRIDGE_FUNC(void, ThreadContext, std::int32_t aHandle, eka2l1::ptr<des8> aContext) {
        kernel_system *kern = sys->get_kernel_system();
        thread_ptr thr = kern->get<kernel::thread>(aHandle);

        if (!thr) {
            return;
        }

        arm::arm_interface::thread_context &ctx = thr->get_thread_context();

        // Save the context, get the fresh one
        sys->get_cpu()->save_context(ctx);

        memory_system *mem = sys->get_memory_system();

        des8 *ctx_epoc_des = aContext.get(mem);
        std::string ctx_epoc_str;
        ctx_epoc_str.resize(sizeof(arm_context_epoc));

        arm_context_epoc *context_epoc = reinterpret_cast<arm_context_epoc *>(&ctx_epoc_str[0]);

        std::copy(ctx.cpu_registers.begin(), ctx.cpu_registers.end(), context_epoc->regs);
        context_epoc->cpsr = ctx.cpsr;
        context_epoc->dacr = 0;

        ctx_epoc_des->assign(kern->crr_process(), ctx_epoc_str);
    }

    BRIDGE_FUNC(void, ThreadRendezvous, std::int32_t aReason) {
        sys->get_kernel_system()->crr_thread()->rendezvous(aReason);
    }

    BRIDGE_FUNC(void, ThreadLogon, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestSts, bool aRendezvous) {
        thread_ptr thr = sys->get_kernel_system()->get<kernel::thread>(aHandle);

        if (!thr) {
            return;
        }

        thr->logon(aRequestSts, aRendezvous);
    }

    BRIDGE_FUNC(std::int32_t, ThreadLogonCancel, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestSts, bool aRendezvous) {
        thread_ptr thr = sys->get_kernel_system()->get<kernel::thread>(aHandle);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        bool logon_success = thr->logon_cancel(aRequestSts, aRendezvous);

        if (logon_success) {
            return epoc::error_none;
        }

        return epoc::error_general;
    }

    BRIDGE_FUNC(void, ThreadSetFlags, std::int32_t aHandle, std::uint32_t aClearMask, std::uint32_t aSetMask) {
        kernel_system *kern = sys->get_kernel_system();

        thread_ptr thr = kern->get<kernel::thread>(aHandle);

        uint32_t org_flags = thr->get_flags();
        uint32_t new_flags = ((org_flags & ~aClearMask) | aSetMask);
        new_flags = (new_flags ^ org_flags);

        thr->set_flags(org_flags ^ new_flags);
    }

    /*****************************/
    /* PROPERTY */
    /****************************/

    BRIDGE_FUNC(std::int32_t, PropertyFindGetInt, std::int32_t aCage, std::int32_t aKey, eka2l1::ptr<std::int32_t> aValue) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        property_ptr prop = kern->get_prop(aCage, aKey);

        if (!prop) {
            LOG_WARN("Property not found: cagetory = 0x{:x}, key = 0x{:x}", aCage, aKey);
            return epoc::error_not_found;
        }

        std::int32_t *val_ptr = aValue.get(mem);
        *val_ptr = prop->get_int();

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, PropertyFindGetBin, std::int32_t aCage, std::int32_t aKey, eka2l1::ptr<std::uint8_t> aData, std::int32_t aDataLength) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        property_ptr prop = kern->get_prop(aCage, aKey);

        if (!prop) {
            LOG_WARN("Property not found: cagetory = 0x{:x}, key = 0x{:x}", aCage, aKey);
            return epoc::error_not_found;
        }

        std::uint8_t *data_ptr = aData.get(mem);
        auto data_vec = prop->get_bin();

        const std::size_t size_to_copy = std::min<std::size_t>(data_vec.size(), aDataLength);
        std::int32_t return_code = epoc::error_none;

        if (data_vec.size() > aDataLength) {
            // The given buffer can't hold ours.
            return_code = epoc::error_overflow;
        }

        // Whether the buffer is too small, we still have to either copy truncated or full data.
        std::copy(data_vec.begin(), data_vec.begin() + size_to_copy, aData.get(mem));

        if (return_code != epoc::error_none) {
            return return_code;
        }
        
        return aDataLength;
    }

    BRIDGE_FUNC(std::int32_t, PropertyAttach, std::int32_t aCagetory, std::int32_t aValue, epoc::owner_type aOwnerType) {
        kernel_system *kern = sys->get_kernel_system();
        property_ptr prop = kern->get_prop(aCagetory, aValue);

        LOG_TRACE("Attach to property with cagetory: 0x{:x}, key: 0x{:x}", aCagetory, aValue);

        if (!prop) {
            prop = kern->create<service::property>();

            if (!prop) {
                return epoc::error_general;
            }

            prop->first = aCagetory;
            prop->second = aValue;
        }

        auto property_ref_handle_and_obj = kern->create_and_add<service::property_reference>(
            static_cast<kernel::owner_type>(aOwnerType), prop);
        
        if (property_ref_handle_and_obj.first == INVALID_HANDLE) {
            return epoc::error_general;
        }

        return property_ref_handle_and_obj.first;
    }

    BRIDGE_FUNC(std::int32_t, PropertyDefine, std::int32_t aCagetory, std::int32_t aKey, eka2l1::ptr<TPropertyInfo> aPropertyInfo) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        TPropertyInfo *info = aPropertyInfo.get(mem);

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

        LOG_TRACE("Define to property with cagetory: 0x{:x}, key: 0x{:x}, type: {}", aCagetory, aKey,
            prop_type == service::property_type::int_data ? "int" : "bin");

        property_ptr prop = kern->get_prop(aCagetory, aKey);

        if (!prop) {
            prop = kern->create<service::property>();

            if (!prop) {
                return epoc::error_general;
            }

            prop->first = aCagetory;
            prop->second = aKey;
        }

        prop->define(prop_type, info->size);

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, PropertySubscribe, std::int32_t aPropertyHandle, eka2l1::ptr<epoc::request_status> aRequestStatus) {
        kernel_system *kern = sys->get_kernel_system();
        property_ref_ptr prop = kern->get<service::property_reference>(aPropertyHandle);

        if (!prop) {
            return;
        }

        epoc::notify_info info(aRequestStatus, kern->crr_thread());
        prop->subscribe(info);
    }

    BRIDGE_FUNC(void, PropertyCancel, std::int32_t aPropertyHandle) {
        kernel_system *kern = sys->get_kernel_system();
        property_ref_ptr prop = kern->get<service::property_reference>(aPropertyHandle);

        if (!prop) {
            return;
        }

        prop->cancel();

        return;
    }

    BRIDGE_FUNC(std::int32_t, PropertySetInt, std::int32_t aHandle, std::int32_t aValue) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ref_ptr prop = kern->get<service::property_reference>(aHandle);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        bool res = prop->get_property_object()->set_int(aValue);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, PropertySetBin, std::int32_t aHandle, std::int32_t aSize, eka2l1::ptr<std::uint8_t> aDataPtr) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ref_ptr prop = kern->get<service::property_reference>(aHandle);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        bool res = prop->get_property_object()->set(aDataPtr.get(mem), aSize);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, PropertyGetInt, std::int32_t aHandle, eka2l1::ptr<std::int32_t> aValuePtr) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ref_ptr prop = kern->get<service::property_reference>(aHandle);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        *aValuePtr.get(mem) = prop->get_property_object()->get_int();

        if (prop->get_property_object()->get_int() == -1) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, PropertyGetBin, std::int32_t aHandle, eka2l1::ptr<std::uint8_t> aDataPtr, std::int32_t aSize) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ref_ptr prop = kern->get<service::property_reference>(aHandle);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        std::vector<uint8_t> dat = prop->get_property_object()->get_bin();

        if (dat.size() == 0) {
            return epoc::error_argument;
        }

        const std::size_t size_to_copy = std::min<std::size_t>(dat.size(), aSize);
        std::int32_t return_code = epoc::error_none;

        if (dat.size() > aSize) {
            // The given buffer can't hold ours.
            return_code = epoc::error_overflow;
        }

        // Whether the buffer is too small, we still have to either copy truncated or full data.
        std::copy(dat.begin(), dat.begin() + size_to_copy, aDataPtr.get(mem));

        if (return_code != epoc::error_none) {
            return return_code;
        }

        return aSize;
    }

    BRIDGE_FUNC(std::int32_t, PropertyFindSetInt, std::int32_t aCage, std::int32_t aKey, std::int32_t aValue) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ptr prop = kern->get_prop(aCage, aKey);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        bool res = prop->set(aValue);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, PropertyFindSetBin, std::int32_t aCage, std::int32_t aKey, eka2l1::ptr<std::uint8_t> aDataPtr, std::int32_t aSize) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        property_ptr prop = kern->get_prop(aCage, aKey);

        if (!prop) {
            return epoc::error_bad_handle;
        }

        bool res = prop->set(aDataPtr.get(mem), aSize);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    /**********************/
    /* TIMER */
    /*********************/
    BRIDGE_FUNC(std::int32_t, TimerCreate) {
        return sys->get_kernel_system()->create_and_add<kernel::timer>(
                                           kernel::owner_type::process, sys->get_timing_system(),
                                           "timer" + common::to_string(eka2l1::random()))
            .first;
    }

    /* 
    * Note: the difference between At and After on hardware is At request still actives when the phone shutdown.
    * At is extremely important to implement the alarm in S60 (i believe S60v4 is a part based on S60 so it maybe related).
    * In emulator, it's the same, so i implement it as TimerAffter.
    */

    BRIDGE_FUNC(void, TimerAfter, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestStatus, std::int32_t aMicroSeconds) {
        kernel_system *kern = sys->get_kernel_system();
        timer_ptr timer = kern->get<kernel::timer>(aHandle);

        if (!timer) {
            return;
        }

        timer->after(kern->crr_thread(), aRequestStatus.get(sys->get_memory_system()), aMicroSeconds);
    }

    BRIDGE_FUNC(void, TimerAtUtc, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestStatus, std::uint64_t aMicroSecondsAt) {
        kernel_system *kern = sys->get_kernel_system();
        timer_ptr timer = kern->get<kernel::timer>(aHandle);

        if (!timer) {
            return;
        }

        timer->after(kern->crr_thread(), aRequestStatus.get(sys->get_memory_system()), aMicroSecondsAt - kern->home_time());
    }

    BRIDGE_FUNC(void, TimerCancel, std::int32_t aHandle) {
        kernel_system *kern = sys->get_kernel_system();
        timer_ptr timer = kern->get<kernel::timer>(aHandle);

        if (!timer) {
            return;
        }

        timer->cancel_request();
    }

    /**********************/
    /* CHANGE NOTIFIER */
    /**********************/
    BRIDGE_FUNC(std::int32_t, ChangeNotifierCreate, epoc::owner_type aOwner) {
        return sys->get_kernel_system()->create_and_add<kernel::change_notifier>(static_cast<kernel::owner_type>(aOwner)).first;
    }

    BRIDGE_FUNC(std::int32_t, ChangeNotifierLogon, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestStatus) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        change_notifier_ptr cnot = kern->get<kernel::change_notifier>(aHandle);

        if (!cnot) {
            return epoc::error_bad_handle;
        }

        bool res = cnot->logon(aRequestStatus);

        if (!res) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    /* MESSAGE QUEUE */
    BRIDGE_FUNC(std::int32_t, MessageQueueNotifyDataAvailable, const std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aStatus) {
        kernel_system *kern = sys->get_kernel_system();
        kernel::thread *request_thread = kern->crr_thread();

        epoc::notify_info info;
        info.requester = request_thread;
        info.sts = aStatus;

        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(aHandle);

        if (!queue) {
            return epoc::error_bad_handle;
        }

        const bool result = queue->notify_available(info);

        if (!result) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    /* DEBUG AND SECURITY */

    BRIDGE_FUNC(void, DebugPrint, eka2l1::ptr<desc8> aDes, std::int32_t aMode) {
        LOG_TRACE("{}",
            aDes.get(sys->get_memory_system())->to_std_string(sys->get_kernel_system()->crr_process()));
    }

    // Let all pass for now
    BRIDGE_FUNC(std::int32_t, PlatSecDiagnostic, eka2l1::ptr<void> aPlatSecInfo) {
        return epoc::error_none;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, GetGlobalUserData) {
        LOG_INFO("GetGlobalUserData stubbed with zero");
        return 0;
    }

    BRIDGE_FUNC(address, ExceptionDescriptor, address aInAddr) {
        // It's not really stable, so for now
        return epoc::get_exception_descriptor_addr(sys, aInAddr);
        // return 0;
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

    BRIDGE_FUNC(std::int32_t, SafeInc32, eka2l1::ptr<std::int32_t> aVal) {
        std::int32_t *val = aVal.get(sys->get_memory_system());
        std::int32_t org_val = *val;
        *val > 0 ? val++ : 0;

        return org_val;
    }

    BRIDGE_FUNC(std::int32_t, SafeDec32, eka2l1::ptr<std::int32_t> aVal) {
        std::int32_t *val = aVal.get(sys->get_memory_system());
        std::int32_t org_val = *val;
        *val > 0 ? val-- : 0;

        return org_val;
    }

    /// HLE
    BRIDGE_FUNC(void, HleDispatch, const std::uint32_t ordinal) {
        sys->get_dispatcher()->resolve(sys, ordinal);
    }

    BRIDGE_FUNC(void, VirtualReality) {
        // Call host function. Hack.
        typedef bool (*reality_func)(void* data);

        const std::uint32_t current = sys->get_cpu()->get_pc();
        std::uint64_t *data = reinterpret_cast<std::uint64_t*>(sys->get_kernel_system()->
            crr_process()->get_ptr_on_addr_space(current - 20));

        sys->get_cpu()->save_context(sys->get_kernel_system()->crr_thread()->get_thread_context());

        reality_func to_call = reinterpret_cast<reality_func>(*data++);
        void *userdata = reinterpret_cast<void*>(*data++);

        if (!to_call(userdata)) {
            sys->get_kernel_system()->crr_thread()->wait_for_any_request();
        }
    }

    /*
    BRIDGE_FUNC(std::int32_t, AtomicTas32, eka2l1::ptr<SAtomicOpInfo32> aAtomicInfo) {
        SAtomicOpInfo32 *info = aAtomicInfo.get(sys->get_memory_system());

        std::int32_t *A = reinterpret_cast<std::int32_t*>(info->iA);
        std::int32_t old = *A;

        (*A >= info->i0) ? (*A += info->i1) : (*A += info->i2);

        return old;
    }
    */

    const eka2l1::hle::func_map svc_register_funcs_v94 = {
        /* FAST EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00800000, WaitForAnyRequest),
        BRIDGE_REGISTER(0x00800001, Heap),
        BRIDGE_REGISTER(0x00800002, HeapSwitch),
        BRIDGE_REGISTER(0x00800005, ActiveScheduler),
        BRIDGE_REGISTER(0x00800006, SetActiveScheduler),
        BRIDGE_REGISTER(0x00800008, TrapHandler),
        BRIDGE_REGISTER(0x00800009, SetTrapHandler),
        BRIDGE_REGISTER(0x0080000C, DebugMask),
        BRIDGE_REGISTER(0x0080000D, DebugMaskIndex),
        BRIDGE_REGISTER(0x0080000E, SetDebugMask),
        BRIDGE_REGISTER(0x00800013, UserSvrRomHeaderAddress),
        BRIDGE_REGISTER(0x00800014, UserSvrRomRootDirAddress),
        BRIDGE_REGISTER(0x00800015, SafeInc32),
        BRIDGE_REGISTER(0x00800019, UTCOffset),
        BRIDGE_REGISTER(0x0080001A, GetGlobalUserData),
        /* SLOW EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00, ObjectNext),
        BRIDGE_REGISTER(0x01, ChunkBase),
        BRIDGE_REGISTER(0x02, ChunkSize),
        BRIDGE_REGISTER(0x03, ChunkMaxSize),
        BRIDGE_REGISTER(0x0C, IMB_Range),
        BRIDGE_REGISTER(0x0E, LibraryLookup),
        BRIDGE_REGISTER(0x11, MutexWait),
        BRIDGE_REGISTER(0x12, MutexSignal),
        BRIDGE_REGISTER(0x13, ProcessGetId),
        BRIDGE_REGISTER(0x14, DllFileName),
        BRIDGE_REGISTER(0x15, ProcessResume),
        BRIDGE_REGISTER(0x16, ProcessFilename),
        BRIDGE_REGISTER(0x17, ProcessCommandLine),
        BRIDGE_REGISTER(0x18, ProcessExitType),
        BRIDGE_REGISTER(0x1C, ProcessSetPriority),
        BRIDGE_REGISTER(0x1E, ProcessSetFlags),
        BRIDGE_REGISTER(0x1F, SemaphoreWait),
        BRIDGE_REGISTER(0x20, SemaphoreSignal),
        BRIDGE_REGISTER(0x21, SemaphoreSignalN),
        BRIDGE_REGISTER(0x22, ServerReceive),
        BRIDGE_REGISTER(0x23, ServerCancel),
        BRIDGE_REGISTER(0x24, SetSessionPtr),
        BRIDGE_REGISTER(0x25, SessionSend),
        BRIDGE_REGISTER(0x26, ThreadID),
        BRIDGE_REGISTER(0x27, SessionShare),
        BRIDGE_REGISTER(0x28, ThreadResume),
        BRIDGE_REGISTER(0x29, ThreadSuspend),
        BRIDGE_REGISTER(0x2B, ThreadSetPriority),
        BRIDGE_REGISTER(0x2F, ThreadSetFlags),
        BRIDGE_REGISTER(0x35, TimerCancel),
        BRIDGE_REGISTER(0x36, TimerAfter),
        BRIDGE_REGISTER(0x37, TimerAtUtc),
        BRIDGE_REGISTER(0x39, ChangeNotifierLogon),
        BRIDGE_REGISTER(0x3B, RequestSignal),
        BRIDGE_REGISTER(0x3C, HandleName),
        BRIDGE_REGISTER(0x3D, HandleFullName),
        BRIDGE_REGISTER(0x40, After),
        BRIDGE_REGISTER(0x42, MessageComplete),
        BRIDGE_REGISTER(0x44, TimeNow),
        BRIDGE_REGISTER(0x4D, SessionSendSync),
        BRIDGE_REGISTER(0x4E, DllTls),
        BRIDGE_REGISTER(0x4F, HalFunction),
        BRIDGE_REGISTER(0x52, ProcessCommandLineLength),
        BRIDGE_REGISTER(0x56, DebugPrint),
        // This call actually is IsExceptionHandled.
        // BRIDGE_REGISTER(0x5E, ThreadContext),
        BRIDGE_REGISTER(0x5F, ProcessGetMemoryInfo),
        BRIDGE_REGISTER(0x6A, HandleClose),
        BRIDGE_REGISTER(0x64, ProcessType),
        BRIDGE_REGISTER(0x68, ThreadCreate),
        BRIDGE_REGISTER(0x6B, ChunkCreate),
        BRIDGE_REGISTER(0x6C, ChunkAdjust),
        BRIDGE_REGISTER(0x6D, HandleOpenObject),
        BRIDGE_REGISTER(0x6E, HandleDuplicate),
        BRIDGE_REGISTER(0x6F, MutexCreate),
        BRIDGE_REGISTER(0x70, SemaphoreCreate),
        BRIDGE_REGISTER(0x73, ThreadKill),
        BRIDGE_REGISTER(0x74, ThreadLogon),
        BRIDGE_REGISTER(0x75, ThreadLogonCancel),
        BRIDGE_REGISTER(0x76, DllSetTls),
        BRIDGE_REGISTER(0x77, DllFreeTLS),
        BRIDGE_REGISTER(0x78, ThreadRename),
        BRIDGE_REGISTER(0x79, ProcessRename),
        BRIDGE_REGISTER(0x7B, ProcessLogon),
        BRIDGE_REGISTER(0x7C, ProcessLogonCancel),
        BRIDGE_REGISTER(0x7D, ThreadProcess),
        BRIDGE_REGISTER(0x7E, ServerCreate),
        BRIDGE_REGISTER(0x7F, SessionCreate),
        BRIDGE_REGISTER(0x80, SessionCreateFromHandle),
        BRIDGE_REGISTER(0x84, TimerCreate),
        BRIDGE_REGISTER(0x87, ChangeNotifierCreate),
        BRIDGE_REGISTER(0x9C, WaitDllLock),
        BRIDGE_REGISTER(0x9D, ReleaseDllLock),
        BRIDGE_REGISTER(0x9E, LibraryAttach),
        BRIDGE_REGISTER(0x9F, LibraryAttached),
        BRIDGE_REGISTER(0xA0, StaticCallList),
        BRIDGE_REGISTER(0xA3, LastThreadHandle),
        BRIDGE_REGISTER(0xA4, ThreadRendezvous),
        BRIDGE_REGISTER(0xA5, ProcessRendezvous),
        BRIDGE_REGISTER(0xA6, MessageGetDesLength),
        BRIDGE_REGISTER(0xA7, MessageGetDesMaxLength),
        BRIDGE_REGISTER(0xA8, MessageIpcCopy),
        BRIDGE_REGISTER(0xAC, MessageKill),
        BRIDGE_REGISTER(0xAE, ProcessSecurityInfo),
        BRIDGE_REGISTER(0xAF, ThreadSecurityInfo),
        BRIDGE_REGISTER(0xB0, MessageSecurityInfo),
        BRIDGE_REGISTER(0xB9, MessageQueueNotifyDataAvailable),
        BRIDGE_REGISTER(0xBC, PropertyDefine),
        BRIDGE_REGISTER(0xBE, PropertyAttach),
        BRIDGE_REGISTER(0xBF, PropertySubscribe),
        BRIDGE_REGISTER(0xC0, PropertyCancel),
        BRIDGE_REGISTER(0xC1, PropertyGetInt),
        BRIDGE_REGISTER(0xC2, PropertyGetBin),
        BRIDGE_REGISTER(0xC3, PropertySetInt),
        BRIDGE_REGISTER(0xC4, PropertySetBin),
        BRIDGE_REGISTER(0xC5, PropertyFindGetInt),
        BRIDGE_REGISTER(0xC6, PropertyFindGetBin),
        BRIDGE_REGISTER(0xC7, PropertyFindSetInt),
        BRIDGE_REGISTER(0xC8, PropertyFindSetBin),
        BRIDGE_REGISTER(0xCF, ProcessSetDataParameter),
        BRIDGE_REGISTER(0xD1, ProcessGetDataParameter),
        BRIDGE_REGISTER(0xD2, ProcessDataParameterLength),
        BRIDGE_REGISTER(0xDB, PlatSecDiagnostic),
        BRIDGE_REGISTER(0xDC, ExceptionDescriptor),
        BRIDGE_REGISTER(0xDD, ThreadRequestSignal),
        BRIDGE_REGISTER(0xDF, LeaveStart),
        BRIDGE_REGISTER(0xE0, LeaveEnd),
        BRIDGE_REGISTER(0xFE, HleDispatch),
        BRIDGE_REGISTER(0xFF, VirtualReality)
    };

    const eka2l1::hle::func_map svc_register_funcs_v93 = {
        /* FAST EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00800000, WaitForAnyRequest),
        BRIDGE_REGISTER(0x00800001, Heap),
        BRIDGE_REGISTER(0x00800002, HeapSwitch),
        BRIDGE_REGISTER(0x00800005, ActiveScheduler),
        BRIDGE_REGISTER(0x00800006, SetActiveScheduler),
        BRIDGE_REGISTER(0x00800008, TrapHandler),
        BRIDGE_REGISTER(0x00800009, SetTrapHandler),
        BRIDGE_REGISTER(0x0080000D, DebugMask),
        /* SLOW EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00, ObjectNext)
    };
}
