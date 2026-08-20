/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <chrono>
#include <common/algorithm.h>
#include <common/platform.h>
#include <common/time.h>
#include <ctime>
#include <cstdlib>
#include <string>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")
#elif EKA2L1_PLATFORM(DARWIN)
#include <CoreFoundation/CoreFoundation.h>
#elif EKA2L1_PLATFORM(POSIX)
#include <limits.h>
#include <unistd.h>
#endif

namespace eka2l1::common {
    std::uint64_t get_current_utc_time_in_microseconds_since_epoch() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::uint64_t get_current_utc_time_in_microseconds_since_0ad() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
            + ad_epoc_dist_microsecs;
    }

    std::uint64_t get_current_utc_time_in_nanoseconds_since_epoch() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::uint64_t convert_microsecs_epoch_to_0ad(const std::uint64_t micsecs) {
        return micsecs * microsecs_per_sec + ad_epoc_dist_microsecs;
    }

    std::uint64_t convert_microsecs_win32_1601_epoch_to_0ad(const std::uint64_t micsecs) {
        return micsecs / 10 + ad_win32_epoch_dist_microsecs;
    }

    int get_current_utc_offset() {
        const std::time_t current_time = std::time(nullptr);
        return get_local_time_zone_info(static_cast<std::int64_t>(current_time)).offset_seconds;
    }

    local_time_zone_info get_local_time_zone_info(const std::int64_t unix_seconds) {
        const std::time_t timestamp = static_cast<std::time_t>(unix_seconds);
        std::tm local_time{};

#if EKA2L1_PLATFORM(WIN32)
        if (localtime_s(&local_time, &timestamp) != 0) {
            return { 0, false };
        }

        std::tm local_copy = local_time;
        const std::time_t local_as_utc = _mkgmtime(&local_copy);
        return {
            static_cast<int>(local_as_utc - timestamp),
            local_time.tm_isdst > 0
        };
#else
        if (!localtime_r(&timestamp, &local_time)) {
            return { 0, false };
        }

        return {
            static_cast<int>(local_time.tm_gmtoff),
            local_time.tm_isdst > 0
        };
#endif
    }

    std::string get_current_time_zone_name() {
        const char *environment_name = std::getenv("TZ");
        if (environment_name && environment_name[0] != '\0' && environment_name[0] != ':') {
            return environment_name;
        }

#if EKA2L1_PLATFORM(WIN32)
        DYNAMIC_TIME_ZONE_INFORMATION info{};
        if (GetDynamicTimeZoneInformation(&info) != TIME_ZONE_ID_INVALID) {
            const int needed = WideCharToMultiByte(CP_UTF8, 0, info.TimeZoneKeyName, -1,
                nullptr, 0, nullptr, nullptr);
            if (needed > 1) {
                std::string result(static_cast<std::size_t>(needed), '\0');
                WideCharToMultiByte(CP_UTF8, 0, info.TimeZoneKeyName, -1,
                    result.data(), needed, nullptr, nullptr);
                result.resize(static_cast<std::size_t>(needed - 1));
                return result;
            }
        }
#elif EKA2L1_PLATFORM(DARWIN)
        CFTimeZoneRef zone = CFTimeZoneCopySystem();
        if (zone) {
            CFStringRef name = CFTimeZoneGetName(zone);
            const CFIndex length = name ? CFStringGetLength(name) : 0;
            const CFIndex maximum = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
            std::string result;

            if (maximum > 1) {
                result.resize(static_cast<std::size_t>(maximum));
                if (CFStringGetCString(name, result.data(), maximum, kCFStringEncodingUTF8)) {
                    result.resize(std::char_traits<char>::length(result.c_str()));
                } else {
                    result.clear();
                }
            }

            CFRelease(zone);
            if (!result.empty()) {
                return result;
            }
        }
#elif EKA2L1_PLATFORM(POSIX)
        char link_target[PATH_MAX + 1]{};
        const ssize_t length = readlink("/etc/localtime", link_target, PATH_MAX);
        if (length > 0) {
            link_target[length] = '\0';
            const std::string path(link_target);
            const std::string marker = "/zoneinfo/";
            const std::size_t marker_position = path.find(marker);
            if (marker_position != std::string::npos) {
                return path.substr(marker_position + marker.size());
            }
        }
#endif

        const std::time_t current_time = std::time(nullptr);
        std::tm local_time{};
#if EKA2L1_PLATFORM(WIN32)
        if (localtime_s(&local_time, &current_time) == 0 && _tzname[0]) {
            return _tzname[local_time.tm_isdst > 0 ? 1 : 0];
        }
#else
        if (localtime_r(&current_time, &local_time) && local_time.tm_zone) {
            return local_time.tm_zone;
        }
#endif
        return "UTC";
    }

    struct basic_teletimer_micro : public teletimer {
        std::uint64_t start_;
        std::uint64_t end_;

        std::uint32_t target_freq_;

    public:
        explicit basic_teletimer_micro(const std::uint32_t freq)
            : target_freq_(freq) {
        }

        ~basic_teletimer_micro() override {
        }

        void start() override {
            start_ = get_current_utc_time_in_microseconds_since_epoch();
            end_ = 0;
        }

        void stop() override {
            end_ = get_current_utc_time_in_microseconds_since_epoch();
        }

        bool set_target_frequency(const std::uint32_t freq) override {
            target_freq_ = freq;
            return true;
        }

        std::uint64_t ticks() override {
            return multiply_and_divide_qwords(microseconds(), target_freq_, 1000000);
        }

        std::uint64_t microseconds() override {
            if (end_ == 0) {
                return get_current_utc_time_in_microseconds_since_epoch() - start_;
            }

            return end_ - start_;
        }

        std::uint64_t nanoseconds() override {
            if (end_ == 0) {
                return get_current_utc_time_in_nanoseconds_since_epoch() - start_;
            }

            return end_ - start_;
        }
    };

    std::unique_ptr<teletimer> make_teletimer(const std::uint32_t target_frequency) {
        return std::make_unique<basic_teletimer_micro>(target_frequency);
    }

#if EKA2L1_PLATFORM(WIN32)
    static constexpr DWORD MILLISECS_SOLUTION_PERIOD_HR = 1;
#endif

    high_resolution_timer_period_guard::high_resolution_timer_period_guard()
        : set_(false) {
    }

    high_resolution_timer_period_guard::~high_resolution_timer_period_guard() {
#if EKA2L1_PLATFORM(WIN32)
        if (set_) {
            timeEndPeriod(MILLISECS_SOLUTION_PERIOD_HR);
        }
#endif
    }

    void high_resolution_timer_period_guard::toogle() {
#if EKA2L1_PLATFORM(WIN32)
        if (set_) {
            timeEndPeriod(MILLISECS_SOLUTION_PERIOD_HR);
        } else {
            timeBeginPeriod(MILLISECS_SOLUTION_PERIOD_HR);
        }
#endif

        set_ = !set_;
    }
}
