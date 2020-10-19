/*
 * Copyright (c) 2019 EKA2L1 Team / 2020 yuzu Emulator Project.
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

#include <common/log.h>
#include <common/platform.h>
#include <common/sync.h>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

namespace eka2l1::common {
    semaphore::semaphore(const int initial)
        : count_(initial) {
    }

    void semaphore::notify(const int count) {
        std::unique_lock<std::mutex> ulock(mut_);
        int to_dec = count;

        while (to_dec-- > 0) {
            count_++;

            if (count_ <= 0) {
                cond_.notify_one();
            }
        }
    }

    bool semaphore::wait(const std::size_t duration_us) {
        std::unique_lock<std::mutex> ulock(mut_);
        count_ -= 1;

        if (count_ < 0) {
            if (duration_us > 0) {
                if (cond_.wait_for(ulock, std::chrono::microseconds(duration_us)) == std::cv_status::timeout) {
                    // Timeout, refund the wait
                    count_++;
                    return true;
                }

                return false;
            }
            
            cond_.wait(ulock);
        }

        return false;
    }

#if EKA2L1_PLATFORM(WIN32)
    class event_impl {
    private:
        HANDLE timer_;
        HANDLE evt_;

    public:
        explicit event_impl() {
            evt_ = CreateEvent(NULL, TRUE, FALSE, NULL);
            timer_ = NULL;

            if (!evt_) {
                LOG_ERROR("Fail to create the Win32 event with error code: {}", GetLastError());
            }
        }

        ~event_impl() {
            CloseHandle(timer_);
            CloseHandle(evt_);
        }

        void set() {
            if (evt_) {
                if (!SetEvent(evt_)) {
                    LOG_ERROR("Fail to set Win32 event with error code: {}", GetLastError());
                }
            }
        }

        void wait() {
            if (WaitForSingleObject(evt_, INFINITE) == WAIT_FAILED) {
                LOG_ERROR("Fail to wait for Win32 event with error code: {}", GetLastError());
            }
        }

        void reset() {
            if (evt_) {
                ResetEvent(evt_);
            }

            if (timer_) {
                CancelWaitableTimer(timer_);
            }
        }

        bool wait_for(const std::uint64_t duration_us) {
            if (timer_) {
                CancelWaitableTimer(timer_);
            } else {
                timer_ = CreateWaitableTimer(NULL, TRUE, NULL);

                if (!timer_) {
                    LOG_ERROR("Fail to create waitable timer for Win32 with error code: {}", GetLastError());
                    return false;
                }
            }

            HANDLE list_to_wait[2] = { evt_, timer_ };

            // The interval is in 100 nanoseconds. 1 microsecs = 1000 nanoseconds
            // => 1 microsecs = 10 interval unit.
            LARGE_INTEGER time_to_pass;
            time_to_pass.QuadPart = -static_cast<std::int64_t>(duration_us * 10);

            if (!SetWaitableTimer(timer_, &time_to_pass, 0, NULL, NULL, FALSE)) {
                LOG_ERROR("Failed to set Win32 waitable timer with error code: {}", GetLastError());
                return false;
            }

            const DWORD result = WaitForMultipleObjects(2, list_to_wait, FALSE, INFINITE);

            switch (result) {
            case WAIT_OBJECT_0:
                // Cancel the pending timer
                CancelWaitableTimer(timer_);
                ResetEvent(evt_);

                return true;

            case WAIT_OBJECT_0 + 1:
                ResetEvent(evt_);
                return false;

            default:
                LOG_ERROR("Waiting for Win32 event with timeout failed with error code: {}", GetLastError());
                break;
            }

            return false;
        }
    };
#else
    class event_impl {
    private:
        bool is_set_;

        std::condition_variable cond_;
        std::mutex lock_;

    public:
        explicit event_impl();

        void set();

        void wait();
        bool wait_for(const std::uint64_t duration_us);

        void reset();
    };

    event_impl::event_impl():
        is_set_(false) {
    }

    void event_impl::set() {
        const std::lock_guard<std::mutex> guard(lock_);

        if (!is_set_) {
            is_set_ = true;
            cond_.notify_one();
        }
    }

    void event_impl::wait() {
        std::unique_lock<std::mutex> unq(lock_);
        cond_.wait(unq, [this] { return is_set_; });

        is_set_ = false;
    }

    bool event_impl::wait_for(const std::uint64_t duration_us) {
        std::unique_lock<std::mutex> unq(lock_);
        
        if (!cond_.wait_for(unq, std::chrono::microseconds(duration_us), [this] { return is_set_; }))
            return false;

        is_set_ = false;
        return true;
    }

    void event_impl::reset() {
        const std::lock_guard<std::mutex> guard(lock_);
        is_set_ = false;
    }
#endif

    event::event() {
        impl_ = new event_impl();
    }

    event::~event() {
        delete impl_;
    }

    void event::set() {
        impl_->set();
    }

    void event::wait() {
        impl_->wait();
    }

    bool event::wait_for(const std::uint64_t duration_us) {
        return impl_->wait_for(duration_us);
    }

    void event::reset() {
        return impl_->reset();
    }
}