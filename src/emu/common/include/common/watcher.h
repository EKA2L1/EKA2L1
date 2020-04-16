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

#include <functional>
#include <memory>
#include <tuple>
#include <string>

namespace eka2l1::common {
    struct directory_watcher_impl;

    enum directory_change_mask {
        directory_change_move = 1 << 0,
        directory_change_attrib = 1 << 1,
        directory_change_last_write = 1 << 2,
        directory_change_last_access = 1 << 3,
        directory_change_creation = 1 << 4
    };

    enum directory_change_action {
        directory_change_action_delete = 1 << 0,
        directory_change_action_created = 1 << 1,
        directory_change_action_moved_from = 1 << 2,
        directory_change_action_moved_to = 1 << 3,
        directory_change_action_modified = 1 << 4
    };

    struct directory_change {
        std::string filename_;
        std::uint32_t change_;
    };

    using directory_changes = std::vector<directory_change>;
    using directory_watcher_callback = std::function<void(void*, directory_changes&)>;
    using directory_watcher_callback_pair = std::pair<directory_watcher_callback, void*>;

    struct directory_watcher_data {
        directory_watcher_callback_pair callback_pair_;
        std::uint32_t filters_;

        explicit directory_watcher_data(directory_watcher_callback callback, void *userdata,
            const std::uint32_t filter_mask)
            : callback_pair_(callback, userdata)
            , filters_(filter_mask) {
        }
    };

    /**
     * \brief Watch a directory changes it content (file name changed, new file created ...)
     */
    struct directory_watcher {
        std::shared_ptr<directory_watcher_impl> watcher_;

    public:
        explicit directory_watcher();

        /**
         * \brief   Watch a directory.
         * 
         * \param   folder                The path to the folder
         * \param   callback              The callback which is invoked on folder changes.
         * \param   callback_userdata     The userdata that will be passed to the callback.
         * \param   filters               Bitmask flags to choose what changes to notify us.
         * 
         * \returns Handle to the watch (> 0), else error code.
         * 
         * \see     unwatch
         */
        std::int32_t watch(const std::string &folder, directory_watcher_callback callback,
            void *callback_userdata, const std::uint32_t filters);

        /**
         * \brief    Unwatch a directory.
         * 
         * \param    watch_handle The handle that was returned from calling watch()
         * 
         * \returns  True on success.
         */
        bool unwatch(const std::int32_t watch_handle);
    };
}