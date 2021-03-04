/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

// Header for resolving SVC call for function which we don't want to implement.
// This exists because of many Symbian devs make their own allocator and switch it
// Some functions are overridden with HLE functions instead of just implementing the svc call
// Because of its complex structure (NMutex, NThread) change over different Symbian update
// But, the public structure still the same for them (RThread, RFastLock), contains only
// a handle. This give us advantage.

#pragma once

#include <utils/chunk.h>
#include <utils/des.h>
#include <utils/dll.h>
#include <utils/handle.h>
#include <utils/reqsts.h>
#include <utils/sec.h>
#include <common/uid.h>

#include <bridge/bridge.h>
#include <kernel/libmanager.h>

#include <utils/err.h>
#include <utils/handle.h>

#include <cstdint>
#include <unordered_map>

#define BRIDGE_REGISTER(func_sid, func)                                               \
    {                                                                                 \
        func_sid, eka2l1::hle::epoc_import_func { eka2l1::hle::bridge(&func), #func } \
    }

#define BRIDGE_FUNC(ret, name, ...) ret name(kernel_system *kern, ##__VA_ARGS__)

namespace eka2l1::kernel {
    struct memory_info;
}

namespace eka2l1::epoc {
    struct ipc_copy_info {
        eka2l1::ptr<std::uint8_t> target_ptr;
        std::int32_t target_length;
        std::int32_t flags;
        std::uint8_t *target_host_ptr;
    };

    // static_assert(sizeof(ipc_copy_info) == 12, "Size of IPCCopy struct is 12");

    static constexpr std::int32_t CHUNK_SHIFT_BY_0 = 0;
    static constexpr std::int32_t CHUNK_SHIFT_BY_1 = static_cast<std::int32_t>(0x80000000);
    static constexpr std::int32_t IPC_DIR_READ = 0;
    static constexpr std::int32_t IPC_DIR_WRITE = 0x10000000;
    static constexpr std::int32_t IPC_HLE_EKA1 = 0x1;

    enum property_type {
        property_type_int,
        property_type_byte_array,
        property_type_text = property_type_byte_array,
        property_type_large_byte_array,
        property_type_large_text = property_type_large_byte_array,
        property_type_limit,
        property_type_mask = 0xff
    };

    struct property_info {
        std::uint32_t attrib;
        std::uint16_t size;
        property_type type;
        epoc::security_policy read_policy;
        epoc::security_policy write_policy;
    };

    struct eka1_executor {
        static constexpr std::uint32_t NO_NAME_AVAIL_ADDR = 0xFFFF0000;

        enum {
            handle_owner_thread = 0x40000000,
            chunk_access_global = 0x1
        };

        enum attribute_epoc6 {
            execute_v6_create_chunk_normal = 0x00,
            execute_v6_create_chunk_normal_global = 0x01,
            execute_v6_open_chunk_global = 0x02,
            execute_v6_chunk_adjust = 0x03,
            execute_v6_compress_heap = 0x04,
            execute_v6_add_logical_device = 0x05,
            execute_v6_free_logical_device = 0x06,
            execute_v6_add_physical_device = 0x08,
            execute_v6_create_logical_channel = 0x0A,
            execute_v6_open_find_handle = 0x0E,
            execute_v6_duplicate_handle = 0x0F,
            execute_v6_close_handle = 0x10,
            execute_v6_create_server_global = 0x15,
            execute_v6_create_session = 0x16,
            execute_v6_create_mutex = 0x17,
            execute_v6_open_mutex_global = 0x18,
            execute_v6_create_sema = 0x19,
            execute_v6_open_sema_global = 0x1A,
            execute_v6_create_timer = 0x1B,
            execute_v6_open_process_by_id = 0x1F,
            execute_v6_rename_process = 0x20,
            execute_v6_logon_process = 0x25,
            execute_v6_logon_cancel_process = 0x26,
            execute_v6_create_thread = 0x27,
            execute_v6_set_initial_parameter_thread = 0x28,
            execute_v6_open_thread = 0x29,
            execute_v6_open_thread_by_id = 0x2A,
            execute_v6_get_thread_own_process = 0x2B,
            execute_v6_rename_thread = 0x2C,
            execute_v6_kill_thread = 0x2D,
            execute_v6_panic_thread = 0x2F,
            execute_v6_logon_thread = 0x30,
            execute_v6_get_heap_thread = 0x32,
            execute_v6_set_tls = 0x34,
            execute_v6_free_tls = 0x35,
            execute_v6_dll_global_allocate = 0x37,
            execute_v6_open_debug = 0x3F,
            execute_v6_close_debug = 0x40,
            execute_v6_undertaker_create = 0x44,
            execute_v6_undertaker_logon = 0x45,
            execute_v6_undertaker_logon_cancel = 0x46,
            execute_v6_create_chunk_double_ended = 0x4C,
            execute_v6_create_chunk_double_ended_global = 0x4D,
            execute_v6_free_up_system = 0x52,           ///< Free system memory (Unconfirmed).
            execute_v6_share_session = 0x54
        };

        enum attribute_epoc81a {
            execute_v81a_create_chunk_normal = 0x00,
            execute_v81a_create_chunk_normal_global = 0x01,
            execute_v81a_open_chunk_global = 0x02,
            execute_v81a_chunk_adjust = 0x03,
            execute_v81a_compress_heap = 0x04,
            execute_v81a_add_logical_device = 0x05,
            execute_v81a_free_logical_device = 0x06,
            execute_v81a_add_physical_device = 0x08,
            execute_v81a_create_logical_channel = 0x0A,
            execute_v81a_open_find_handle = 0x0E,
            execute_v81a_duplicate_handle = 0x0F,
            execute_v81a_close_handle = 0x10,
            execute_v81a_create_server_global = 0x15,
            execute_v81a_create_session = 0x16,
            execute_v81a_create_mutex = 0x17,
            execute_v81a_open_mutex_global = 0x18,
            execute_v81a_create_sema = 0x19,
            execute_v81a_open_sema_global = 0x1A,
            execute_v81a_create_timer = 0x1B,
            execute_v81a_open_process_by_id = 0x1F,
            execute_v81a_rename_process = 0x20,
            execute_v81a_logon_process = 0x25,
            execute_v81a_logon_cancel_process = 0x26,
            execute_v81a_rendezvous_request_process = 0x27,
            execute_v81a_rendezvous_cancel_process = 0x28,
            execute_v81a_rendezvous_complete_process = 0x29,
            execute_v81a_create_thread = 0x2A,
            execute_v81a_set_initial_parameter_thread = 0x2B,
            execute_v81a_open_thread = 0x2C,
            execute_v81a_open_thread_by_id = 0x2D,
            execute_v81a_get_thread_own_process = 0x2E,
            execute_v81a_rename_thread = 0x2F,
            execute_v81a_kill_thread = 0x30,
            execute_v81a_terminate_thread = 0x31,
            execute_v81a_panic_thread = 0x32,
            execute_v81a_logon_thread = 0x33,
            execute_v81a_logon_cancel = 0x34,
            execute_v81a_rendezvous_request_thread = 0x35,
            execute_v81a_rendezvous_request_cancel_thread = 0x36,
            execute_v81a_rendezvous_request_complete_thread = 0x37,
            execute_v81a_get_heap_thread = 0x38,
            execute_v81a_msg2_kill_sender = 0x3A,
            execute_v81a_set_tls = 0x3C,
            execute_v81a_free_tls = 0x3D,
            execute_v81a_dll_global_allocate = 0x3F,
            execute_v81a_open_debug = 0x47,
            execute_v81a_close_debug = 0x48,
            execute_v81a_undertaker_create = 0x4C,
            execute_v81a_undertaker_logon = 0x4D,
            execute_v81a_undertaker_logon_cancel = 0x4E,
            execute_v81a_create_chunk_double_ended = 0x54,
            execute_v81a_create_chunk_double_ended_global = 0x55,
            execute_v81a_share_session = 0x5A,
            execute_v81a_property_define = 0x65
        };

        std::uint32_t arg0_;
        std::uint32_t arg1_;
        std::uint32_t arg2_;
        std::uint32_t arg3_;
    };

    struct eka1_thread_create_description {
        address func_;
        std::uint32_t stack_size_;
        address heap_;
        std::uint32_t min_heap_size_;
        std::uint32_t max_heap_size_;
        address func_data_;
        address lib_of_func_ptr_;
    };

    enum debug_cmd_opcode {
        debug_cmd_opcode_read = 8,
        debug_cmd_opcode_write = 9,
        debug_cmd_opcode_print = 14
    };

    struct debug_cmd_header {
        std::uint32_t opcode_;
        std::uint32_t thread_id_;
    };

    ///> @brief The SVC map for Symbian S60v3.
    extern const eka2l1::hle::func_map svc_register_funcs_v93;

    ///> @brief The SVC map for Symbian S60v5.
    extern const eka2l1::hle::func_map svc_register_funcs_v94;

    ///> @brief The SVC map for Symbian S^3.
    extern const eka2l1::hle::func_map svc_register_funcs_v10;

    ///> @brief The SVC map for Symbian 8.1a.
    extern const eka2l1::hle::func_map svc_register_funcs_v81a;

    ///> @brief The SVC map for Symbian 6.0.
    extern const eka2l1::hle::func_map svc_register_funcs_v6;
}
