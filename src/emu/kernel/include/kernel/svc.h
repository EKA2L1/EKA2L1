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

#include <epoc/hal.h>
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

        enum attribute {
            handle_owner_thread = 0x40000000,
            chunk_access_global = 0x1,
            execute_create_chunk_normal = 0x00,
            execute_create_chunk_normal_global = 0x01,
            execute_open_chunk_global = 0x02,
            execute_chunk_adjust = 0x03,
            execute_compress_heap = 0x04,
            execute_open_handle = 0x0E,
            execute_duplicate_handle = 0x0F,
            execute_close_handle = 0x10,
            execute_create_server_global = 0x15,
            execute_create_session = 0x16,
            execute_create_mutex = 0x17,
            execute_open_mutex_global = 0x18,
            execute_create_sema = 0x19,
            execute_open_sema_global = 0x1A,
            execute_create_timer = 0x1B,
            execute_rename_process = 0x20,
            execute_create_thread = 0x27,
            execute_open_thread = 0x29,
            execute_open_thread_by_id = 0x2A,
            execute_rename_thread = 0x2C,
            execute_kill_thread = 0x2D,
            execute_panic_thread = 0x2F,
            execute_logon_thread = 0x30,
            execute_get_heap_thread = 0x32,
            execute_set_tls = 0x34,
            execute_free_tls = 0x35,
            execute_open_debug = 0x3F,
            execute_close_debug = 0x40,
            execute_create_chunk_double_ended = 0x4C,
            execute_create_chunk_double_ended_global = 0x4D,
            execute_free_up_system = 0x52           ///< Free system memory (Unconfirmed).
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

    ///> @brief The SVC map for Symbian S60v3.
    extern const eka2l1::hle::func_map svc_register_funcs_v93;

    ///> @brief The SVC map for Symbian S60v5.
    extern const eka2l1::hle::func_map svc_register_funcs_v94;

    ///> @brief The SVC map for Symbian S^3.
    extern const eka2l1::hle::func_map svc_register_funcs_v10;

    ///> @brief The SVC map for Symbian 6.0.
    extern const eka2l1::hle::func_map svc_register_funcs_v6;
}
