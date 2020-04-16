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

#include "watcher_win32.h"

#include <common/cvt.h>
#include <common/log.h>

namespace eka2l1::common {
    static constexpr std::size_t FILE_INFO_MAX_SIZE = sizeof(FILE_NOTIFY_INFORMATION) + 16;
    static constexpr std::size_t MAX_FILE_INFO_COUNT = 512;

    directory_watcher_impl::directory_watcher_impl() {
        HANDLE stop_event = CreateEvent(nullptr, true, false, TEXT("EDirectoryWatcherStopEvt"));

        if (!stop_event || (stop_event == INVALID_HANDLE_VALUE)) {
            LOG_ERROR("Can't create the event to stop directory watcher thread, abort");
            return;
        }

        // Create event for notifing new watches
        HANDLE new_watch_event = CreateEvent(nullptr, true, false, TEXT("EDirectoryWatcherNewWatchEvt"));

        if (!new_watch_event || (new_watch_event == INVALID_HANDLE_VALUE)) {
            LOG_ERROR("Can't create the event to notify new watch for watcher thread, abort");
            CloseHandle(stop_event);

            return;
        }

        // Add two event in wait list
        waits_.push_back(stop_event);
        waits_.push_back(new_watch_event);

        file_infos_.resize(MAX_FILE_INFO_COUNT * FILE_INFO_MAX_SIZE);

        wait_thread_ = std::make_unique<std::thread>([this]() {
            while (true) {
                DWORD which = WaitForMultipleObjects(static_cast<DWORD>(waits_.size()), waits_.data(),
                    FALSE, INFINITE);

                switch (which) {
                case WAIT_OBJECT_0:
                    // This is the stop event. End this loop.
                    break;

                case WAIT_OBJECT_0 + 1:
                    // New watch was added. This is just a simple notifcation.
                    proceed_on_();
                    ResetEvent(waits_[1]);
                    continue;

                case WAIT_TIMEOUT:
                    LOG_WARN("Waiting for a directory changes timed out.");
                    break;

                default: {
                    DWORD buffer_wrote_length = 0;
                    auto &callback = callbacks_[which - WAIT_OBJECT_0 - 2];

                    if (!ReadDirectoryChangesW(dirs_[which - WAIT_OBJECT_0 - 2], &file_infos_[0], static_cast<DWORD>(file_infos_.size()),
                            false, callback.filters_, &buffer_wrote_length, nullptr, nullptr)) {
                        LOG_WARN("Can't read directory changes. Report empty changes.");
                    }

                    std::size_t pointee = 0;
                    directory_changes changes;

                    while (pointee < buffer_wrote_length) {
                        FILE_NOTIFY_INFORMATION *info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(&file_infos_[pointee]);

                        std::u16string filename(reinterpret_cast<char16_t*>(info->FileName), info->FileNameLength);
                        directory_change change;
                        change.change_ = 0;
                        change.filename_ = common::ucs2_to_utf8(filename);

                        switch (info->Action) {
                        case FILE_ACTION_ADDED:
                            change.change_ |= directory_change_action_created;
                            break;

                        case FILE_ACTION_REMOVED:
                            change.change_ |= directory_change_action_delete;
                            break;

                        case FILE_ACTION_RENAMED_NEW_NAME:
                            change.change_ |= directory_change_action_moved_to;
                            break;

                        case FILE_ACTION_RENAMED_OLD_NAME:
                            change.change_ |= directory_change_action_moved_from;
                            break;

                        case FILE_ACTION_MODIFIED:
                            change.change_ |= directory_change_action_modified;
                            break;

                        default:
                            break;
                        }

                        changes.push_back(change);
                        pointee += info->NextEntryOffset;

                        if (info->NextEntryOffset == 0) {
                            break;
                        }
                    }

                    // Invoke callback
                    callback.callback_pair_.first(callback.callback_pair_.second, changes);

                    // Register again
                    if (!FindNextChangeNotification(waits_[which - WAIT_OBJECT_0])) {
                        LOG_WARN("Can't find next change notifications, closing it down!");
                        unwatch(waits_[which - WAIT_OBJECT_0], true);
                    }

                    break;
                }
                }
            }
        });
    }

    directory_watcher_impl::~directory_watcher_impl() {
        const std::lock_guard<std::mutex> guard(lock_);
        SetEvent(waits_[0]);

        // Wait for the thread to terminate
        wait_thread_->join();

        // Close down all remaining events
        CloseHandle(waits_[0]);
        CloseHandle(waits_[1]);

        for (auto &wait = waits_.begin() + 2; wait < waits_.end(); wait++) {
            FindCloseChangeNotification(*wait);
        }
    }

    bool directory_watcher_impl::unwatch(const std::int32_t watch_handle) {
        if ((watch_handle <= 2) || (watch_handle > container_.size()) || 
            container_[watch_handle - 1] == nullptr) {
            return false;
        }

        return unwatch(container_[watch_handle - 1], false);
    }

    static const DWORD convert_to_win32_notify_mask(const std::uint32_t masks) {
        DWORD result = 0;

        if (masks & directory_change_move) {
            result |= FILE_NOTIFY_CHANGE_FILE_NAME;
        }

        if (masks & directory_change_attrib) {
            result |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
        }

        if (masks & directory_change_last_write) {
            result |= FILE_NOTIFY_CHANGE_LAST_WRITE;
        }

        if (masks & directory_change_last_access) {
            result |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
        }

        if (masks & directory_change_creation) {
            result |= FILE_NOTIFY_CHANGE_CREATION;
        }

        return result;
    }

    std::int32_t directory_watcher_impl::watch(const std::string &folder, directory_watcher_callback callback,
        void *callback_userdata, const std::uint32_t masks) {
        const std::lock_guard<std::mutex> guard(lock_);
        HANDLE h = FindFirstChangeNotificationA(folder.c_str(), false, FILE_NOTIFY_CHANGE_FILE_NAME);

        if (!h || h == INVALID_HANDLE_VALUE) {
            LOG_ERROR("Can't create directory watch of folder {}", folder);
            return 0;
        }

        HANDLE dir_handle = CreateFileA(folder.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS,
            nullptr);

        if (dir_handle == INVALID_HANDLE_VALUE) {
            LOG_ERROR("Can't open directory handle");
            FindCloseChangeNotification(h);
            return 0;
        }

        HANDLE added_nof = CreateEvent(NULL, true, false, NULL);
        assert(added_nof != INVALID_HANDLE_VALUE);

        std::int32_t slot = 0;

        // Find slot in container
        proceed_on_ = [&]() {
            auto empty_ite = std::find(container_.begin(), container_.end(), nullptr);

            if (empty_ite == container_.end()) {
                container_.push_back(h);
                slot = static_cast<std::int32_t>(container_.size());
            } else {
                *empty_ite = h;
                slot = static_cast<std::int32_t>(std::distance(container_.begin(), empty_ite) + 1);
            }

            waits_.push_back(h);
            callbacks_.emplace_back(callback, callback_userdata, convert_to_win32_notify_mask(masks));
            dirs_.push_back(dir_handle);

            SetEvent(added_nof);
        };

        SetEvent(waits_[1]);

        WaitForSingleObject(added_nof, INFINITE);
        CloseHandle(added_nof);

        return 0;
    }
    
    bool directory_watcher_impl::unwatch(HANDLE h, const bool is_in_loop) {
        const std::lock_guard<std::mutex> guard(lock_);

        // Find the handle in the lookup table and empty it
        auto con_ite = std::find(container_.begin(), container_.end(), h);

        if (con_ite == container_.end()) {
            return false;
        }

        *con_ite = nullptr;

        HANDLE removed_nof = nullptr;
        bool result = true;
        
        if (!is_in_loop) {
            removed_nof = CreateEvent(NULL, true, false, NULL);
            assert(removed_nof != INVALID_HANDLE_VALUE);
        }

        // Remove the callback and the waits
        proceed_on_ = [&]() {
            if (!FindCloseChangeNotification(h)) {
                LOG_ERROR("Can't close watch handle");
                result = false;

                if (removed_nof)
                    SetEvent(removed_nof);

                return;
            }

            auto wait_ite = std::find(waits_.begin(), waits_.end(), h);

            if (wait_ite == waits_.end()) {
                LOG_ERROR("This should not happens, a wait object not in wait list but in container list");
                
                result = false;

                if (removed_nof)
                    SetEvent(removed_nof);

                return;
            }

            const std::size_t index = std::distance(waits_.begin(), wait_ite);
            waits_.erase(wait_ite);

            // Want to also erase the callback
            callbacks_.erase(callbacks_.begin() + index - 2);
            dirs_.erase(dirs_.begin() + index - 2);

            if (removed_nof)
                SetEvent(removed_nof);
        };
        
        if (!is_in_loop) {
            // Notify the wait thread, specifically the watch list change event
            SetEvent(waits_[1]);

            WaitForSingleObject(removed_nof, INFINITE);
            CloseHandle(removed_nof);
        } else {
            proceed_on_();
        }

        return result;
    }
}