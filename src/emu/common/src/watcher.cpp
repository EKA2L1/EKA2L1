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

#include <common/platform.h>
#include <common/watcher.h>

#if EKA2L1_PLATFORM(WIN32)
#include "watcher_win32.h"
#else
    #if EKA2L1_PLATFORM(UNIX)
    #include "watcher_unix.h"
    #else
    #include "watcher_null.h"
    #endif
#endif

namespace eka2l1::common {
    directory_watcher::directory_watcher() {
        watcher_ = std::make_shared<directory_watcher_impl>();
    }

    std::int32_t directory_watcher::watch(const std::string &folder, directory_watcher_callback callback,
        void *callback_userdata, const std::uint32_t mask) {
        return watcher_->watch(folder, callback, callback_userdata, mask);        
    }

    bool directory_watcher::unwatch(const std::int32_t watch_handle) {
        return watcher_->unwatch(watch_handle);
    }
}
