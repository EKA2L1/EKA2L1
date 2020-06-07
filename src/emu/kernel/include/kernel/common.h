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

    struct raw_event {
        raw_event_type type_;
    };
}