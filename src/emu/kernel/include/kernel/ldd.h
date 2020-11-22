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

#include <kernel/kernel_obj.h>
#include <mem/ptr.h>

#include <utils/reqsts.h>
#include <utils/version.h>

#include <memory>
#include <string>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    class system;
}

namespace eka2l1::ldd {
    class channel: public kernel::kernel_obj {
    protected:
        system *sys_;
        epoc::version ver_;

    public:
        explicit channel(kernel_system *kern, system *sys, epoc::version ver);
        ~channel() override {}
        
        virtual std::int32_t do_control(kernel::thread *r, const std::uint32_t n,
            const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2) = 0;

        virtual std::int32_t do_request(epoc::notify_info info, const std::uint32_t n,
            const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
            const bool is_supervisor) = 0;
    };

    /**
     * @brief Class to construct channel.
     * 
     * Can have a name to identify channel category.
     */
    class factory: public kernel::kernel_obj {
    protected:
        system *sys_;

    public:
        explicit factory(kernel_system *kern, system *sys);

        ~factory() override {}

        virtual void install() = 0;
        virtual std::unique_ptr<channel> make_channel(epoc::version ver) = 0;
    };

    using factory_instance = std::unique_ptr<factory>;
}