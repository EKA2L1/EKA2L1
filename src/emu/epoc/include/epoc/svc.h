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
// Some functions are overriden with HLE functions instead of just implementing the svc call
// Because of its complex structure (NMutex, NThread) change over different Symbian update
// But, the public structure still the same for them (RThread, RFastLock), contains only
// a handle. This give us advantage.

#pragma once

#include <epoc/utils/chunk.h>
#include <epoc/utils/des.h>
#include <epoc/utils/dll.h>
#include <epoc/utils/handle.h>
#include <epoc/utils/reqsts.h>
#include <epoc/utils/sec.h>
#include <epoc/utils/tl.h>
#include <epoc/utils/uid.h>

#include <hle/bridge.h>

#include <epoc/hal.h>
#include <epoc/kernel/libmanager.h>

#include <epoc/utils/err.h>
#include <epoc/utils/handle.h>

#include <cstdint>
#include <unordered_map>

namespace eka2l1::kernel {
    struct memory_info;
}

namespace eka2l1::epoc {
    struct ipc_copy_info {
        eka2l1::ptr<std::uint8_t> target_ptr;
        std::int32_t target_length;
        std::int32_t flags;
    };

    static_assert(sizeof(ipc_copy_info) == 12, "Size of IPCCopy struct is 12");

    static constexpr std::int32_t CHUNK_SHIFT_BY_0 = 0;
    static constexpr std::int32_t CHUNK_SHIFT_BY_1 = static_cast<std::int32_t>(0x80000000);
    static constexpr std::int32_t IPC_DIR_READ = 0;
    static constexpr std::int32_t IPC_DIR_WRITE = 0x10000000;

    enum class exit_type {
        kill,
        terminate,
        panic,
        pending
    };

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

    //! The SVC map for Symbian S60v3.
    extern const eka2l1::hle::func_map svc_register_funcs_v93;

    //! The SVC map for Symbian S60v5
    extern const eka2l1::hle::func_map svc_register_funcs_v94;
}
