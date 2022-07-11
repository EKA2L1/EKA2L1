/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <memory>
#include <string>

namespace eka2l1::config {
    struct state;
}

namespace eka2l1::epoc::bt {
    enum midman_type {
        MIDMAN_PHYSICAL_BT = 0,
        MIDMAN_INET_BT = 1
    };

    /**
     * @brief Middle layer for use of communicating between host OS and
     *        guest OS's bluetooth stack.
     */
    class midman {
    public:
        static constexpr std::size_t MAX_PORT = 60;

    protected:
        std::u16string local_name_;
        void *native_handle_;

    public:
        explicit midman();

        std::u16string device_name() const {
            return local_name_;
        }

        void device_name(const std::u16string &name) {
            local_name_ = name;
        }

        virtual midman_type type() const = 0;
    };

    std::unique_ptr<midman> make_bluetooth_midman(const eka2l1::config::state &conf, const std::uint32_t reserved_stack_type = 0);
}