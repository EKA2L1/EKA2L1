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

#include "watcher_unix.h"
#include <common/log.h>

#include <sys/inotify.h>

namespace eka2l1::common {
    static constexpr std::size_t EVENT_MAX_SIZE = sizeof(struct inotify_event) + 16;

    directory_watcher_impl::directory_watcher_impl()
        : should_stop(false) {
        instance_ = inotify_init();

        if (instance_ == -1) {
            LOG_ERROR("Error creating INotify instance!");
            return;
        }

        // 512 is maximum event count
        events_.resize(EVENT_MAX_SIZE * 512);
        
        wait_thread_ = std::make_unique<std::thread>([this]() {
            std::vector<directory_change> changes;

            auto flush_changes = [&](const int wd) {
                // Flush changes
                auto ite = std::find(container_.begin(), container_.end(), wd);

                if (ite != container_.end()) {
                    auto &callback = callbacks_[std::distance(container_.begin(), ite)];
                    callback.callback_pair_.first(callback.callback_pair_.second, changes);

                    changes.clear();
                }
            };

            while (!should_stop) {
                const ssize_t length = read(instance_, &events_[0], events_.size());
                
                if (length == -1) {
                    LOG_ERROR("Error reading notify event!");
                    should_stop = true;
                }

                std::size_t i = 0;

                int last_wd = -1;

                // Parse all event
                while (i < length) {
                    struct inotify_event *evt = reinterpret_cast<struct inotify_event*>(&events_[i]);
                    
                    directory_change change;
                    change.change_ = 0;
                    change.filename_.assign(evt->name, evt->name + evt->len);
                    
                    // Remove null terminated characters
                    change.filename_.erase(std::find(change.filename_.begin(), change.filename_.end(), '\0'),
                        change.filename_.end());

                    if (evt->mask & IN_MOVED_FROM) {
                        change.change_ |= directory_change_action_moved_from;
                    }

                    if (evt->mask & IN_MOVED_TO) {
                        change.change_ |= directory_change_action_moved_to;
                    }

                    if (evt->mask & IN_MODIFY) {
                        change.change_ |= directory_change_action_modified;
                    }

                    if (evt->mask & IN_CREATE) {
                        change.change_ |= directory_change_action_created;
                    }

                    if (evt->mask & IN_DELETE) {
                        change.change_ |= directory_change_action_delete;
                    }

                    if (evt->mask & IN_ATTRIB) {
                        change.change_ |= directory_change_action_modified;
                    }

                    changes.push_back(change);

                    if ((last_wd != -1) && (last_wd != evt->wd)) {
                        flush_changes(evt->wd);
                    }

                    last_wd = evt->wd;
                    i += evt->len + sizeof(struct inotify_event);
                }

                if (changes.size() != 0) {
                    flush_changes(last_wd);
                }
            }
        });
    }

    directory_watcher_impl::~directory_watcher_impl() {
        for (auto &wd: container_) {
            inotify_rm_watch(instance_, wd);
        }

        should_stop = true;
        wait_thread_->join();

        close(instance_);
    }

    bool directory_watcher_impl::unwatch(const std::int32_t watch_handle) {
        // Find in container
        auto ite = std::find(container_.begin(), container_.end(), watch_handle);

        if (ite == container_.end()) {
            return false;
        }
    
        auto callback_ite = callbacks_.begin() + std::distance(container_.begin(), ite);
        
        callbacks_.erase(callback_ite);
        container_.erase(ite);

        const bool remove_result = (inotify_rm_watch(instance_, watch_handle) != -1);

        if (!remove_result) {
            LOG_WARN("Can not removing watch from inotify!");
        }

        return true;
    }

    static int convert_to_unix_notify_mask(const std::uint32_t masks) {
        int filters = 0;

        if (masks & directory_change_move) {
            filters |= (IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE);
        }

        if ((masks & directory_change_last_access) || (masks & directory_change_last_write)) {
            filters |= IN_ATTRIB;

            if (masks & directory_change_last_write) {
                filters |= IN_MODIFY;
            }
        }

        if (masks & directory_change_creation) {
            filters |= IN_CREATE;
        }

        return filters;
    }

    std::int32_t directory_watcher_impl::watch(const std::string &folder, directory_watcher_callback callback,
        void *callback_userdata, const std::uint32_t mask) {
        const int wd_handle = inotify_add_watch(instance_, folder.c_str(), IN_CREATE | IN_DELETE | IN_MODIFY);

        if (wd_handle == -1) {
            LOG_ERROR("Error creating new inotify watch!");
            return 0;
        }

        container_.push_back(wd_handle);
        callbacks_.emplace_back(callback, callback_userdata, convert_to_unix_notify_mask(mask));

        return wd_handle;
    }
}