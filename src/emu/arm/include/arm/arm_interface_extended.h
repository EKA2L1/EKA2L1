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

#include <arm/arm_interface.h>
#include <cstdint>
#include <map>

namespace eka2l1 {
    class gdbstub;
    class manager_system;

    namespace manager {
        struct config_state;
    }

    class kernel_system;

    namespace kernel {
        class thread;
    }
}

namespace eka2l1::arm {
    class arm_interface_extended: public arm_interface {
    protected:
        gdbstub *stub_;
        manager_system *mngr_;

        struct breakpoint_hit_info {
            bool hit_;
            std::uint32_t addr_;
        };

        std::map<std::uint32_t, breakpoint_hit_info> last_breakpoint_script_hits_;

    public:
        explicit arm_interface_extended(gdbstub *stub, manager_system *mngr);

        virtual void handle_breakpoint(kernel_system *kern, manager::config_state *conf);
        virtual bool last_script_breakpoint_hit(kernel::thread *thr);
        virtual void reset_breakpoint_hit(kernel_system *kern);

        bool is_extended() const override {
            return true;
        }
    };
}