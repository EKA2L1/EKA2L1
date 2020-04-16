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

#include <common/thread.h>
#include <common/watcher.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>

namespace eka2l1::common {
    struct directory_watcher_impl {
        std::unique_ptr<std::thread> wait_thread_;
        std::vector<std::uint8_t> events_;

        int instance_;

        std::atomic<bool> should_stop;

        std::vector<int> container_;
        std::vector<directory_watcher_data> callbacks_;

        std::mutex lock_;

    public:
        explicit directory_watcher_impl();
        ~directory_watcher_impl();
        
        std::int32_t watch(const std::string &folder, directory_watcher_callback callback,
            void *callback_userdata, const std::uint32_t mask);

        bool unwatch(const std::int32_t watch_handle);
    };
}