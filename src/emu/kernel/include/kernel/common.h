/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#pragma once

#include <mem/ptr.h>

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace eka2l1 {
    class kernel_system;

    namespace kernel {
        class process;
    }

    namespace arm {
        class core;
    }
}

namespace eka2l1::hle {
    struct epoc_import_func {
        std::function<void(kernel_system *, kernel::process *, arm::core *)> func;
        std::string name;
    };

    using func_map = std::map<uint32_t, eka2l1::hle::epoc_import_func>;
}

namespace eka2l1::kernel {
    enum class entity_exit_type {
        kill = 0,
        terminate = 1,
        panic = 2,
        pending = 3
    };
    
    enum raw_event_type {
        raw_event_type_redraw = 5
    };

    enum hal_data_eka1 {
        #define HAL_ENTRY(short_name, detail_name, ord, ord_old) hal_data_eka1_##short_name = ord_old,
        #include <kernel/hal.def>
        #undef HAL_ENTRY
    };

    struct raw_event {
        raw_event_type type_;
    };

    struct epoc9_thread_create_info {
        std::int32_t handle;
        std::int32_t type;
        address func_ptr;
        address ptr;
        address supervisor_stack;
        std::int32_t supervisor_stack_size;
        address user_stack;
        std::int32_t user_stack_size;
        std::int32_t init_thread_priority;
        std::uint32_t name_len;
        address name_ptr;
        std::int32_t total_size;
    };

    struct epoc9_std_epoc_thread_create_info : public epoc9_thread_create_info {
        address allocator;
        std::int32_t heap_min;
        std::int32_t heap_max;
        std::int32_t padding;
    };

    enum dll_reason {
        dll_reason_process_attach = 0,
        dll_reason_thread_attach = 1,
        dll_reason_process_detach = 2,
        dll_reason_thread_detach = 3
    };

    // o__o
    struct trap {
        enum {
            TRAP_MAX_STATE = 0x10
        };

        std::int32_t state_[TRAP_MAX_STATE];
        address next_;                ///< Next trap address.
        address result_;              ///< Trap result code pointer.
        address trap_handler_;              ///< Pointer to trap handler.
    };

    struct char_set {
        address char_data_set_;
        address collation_data_set_;
    };
}