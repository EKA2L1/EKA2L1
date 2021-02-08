/*
 * Copyright (c) 2019 EKA2L1 Team.
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
#include <memory>
#include <string>

namespace eka2l1::kernel {
    class codeseg;
}

namespace eka2l1::scripting {
    class process;
    using process_inst = std::shared_ptr<process>;

    class codeseg {
    public:
        kernel::codeseg *real_seg_;

    public:
        codeseg() = default;
        explicit codeseg(std::uint64_t handle);

        std::uint32_t lookup(process_inst pr, const std::uint32_t ord);
        std::uint32_t code_run_address(process_inst pr);
        std::uint32_t data_run_address(process_inst pr);
        std::uint32_t bss_run_address(process_inst pr);

        std::uint32_t code_size();
        std::uint32_t data_size();
        std::uint32_t bss_size();

        std::uint32_t get_export_count();
    };

    std::unique_ptr<scripting::codeseg> load_codeseg(const std::string &virt_path);
}
