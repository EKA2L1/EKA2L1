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

#include <utils/consts.h>
#include <utils/des.h>
#include <utils/reqsts.h>

#include <cstdint>
#include <memory>

namespace eka2l1 {
    class kernel_system;
}

namespace eka2l1::epoc::notifier {
    class plugin_base {
    protected:
        kernel_system *kern_;

    public:
        explicit plugin_base(kernel_system *kern)
            : kern_(kern) {
        }

        /**
         * @brief       Get the unique id of this plugin.
         * @returns     UID of the plugin.
         */
        virtual epoc::uid unique_id() const = 0;

        /**
         * @brief       Handle request from a notifier.
         * 
         * This call must copied neccessary data if executed asynchronously.
         * 
         * @param       request         Pointer to the descriptor containing the requested data.
         * @param       respone         Pointer to the descriptor containing the target respone.
         * @param       complete_info   On finish, this request info is finished with code error_none.
         * 
         * @internal
         */
        virtual void handle(epoc::desc8 *request, epoc::des8 *respone, epoc::notify_info &complete_info) = 0;

        /**
         * @brief       Cancel outstanding handle request.
         */
        virtual void cancel() = 0;
    };

    using plugin_instance = std::unique_ptr<plugin_base>;
};