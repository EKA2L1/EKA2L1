/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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
#include <core/abi/eabi.h>
#include <common/advstream.h>
#include <common/log.h>

#include <mutex>
#include <sstream>
#include <vector>

namespace eka2l1 {
    // This is not exactly an ABI, but a ABI-like
    // implementation of some Symbian specific stuffs
    namespace eabi {
        // Some functions leave.
        struct leave_handler {
            std::string leave_msg;
            uint32_t leave_id;
        };

        std::vector<leave_handler> leaves;
        std::mutex mut;

        void leave(uint32_t id, const std::string &msg) {
            std::lock_guard<std::mutex> guard(mut);
            leave_handler handler;

            handler.leave_id = id;
            handler.leave_msg = msg;

            leaves.push_back(handler);

            LOG_CRITICAL("Process leaved! ID: {}, Message: {}", id, msg);
        }

        void trap_leave(uint32_t id) {
            auto res = std::find_if(leaves.begin(), leaves.end(),
                [id](auto lh) { return lh.leave_id == id; });

            if (res != leaves.end()) {
                std::lock_guard<std::mutex> guard(mut);
                leaves.erase(res);

                LOG_INFO("Leave ID {} has been trapped", id);
            }
        }

        void pure_virtual_call() {
            LOG_CRITICAL("Weird behavior: A pure virtual call called.");
        };

        void deleted_virtual_call() {
            LOG_CRITICAL("Weird behavior: A deleted virtual call called!");
        }
    }
}
