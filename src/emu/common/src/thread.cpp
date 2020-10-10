/*
 * Copyright (c) 2019 EKA2L1 Team, 2013 Dolphin Emulator Project,
 * 2020 Yuzu Emulator Project.
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
#include <cstdint>

#if EKA2L1_PLATFORM(WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <common/cvt.h>
#include <common/thread.h>

namespace eka2l1::common {
#if EKA2L1_PLATFORM(WIN32)
    void set_thread_priority(const thread_priority pri) {
        auto handle = GetCurrentThread();
        int windows_priority = 0;

        switch (pri) {
        case thread_priority_low:
            windows_priority = THREAD_PRIORITY_BELOW_NORMAL;
            break;
        case thread_priority_normal:
            windows_priority = THREAD_PRIORITY_NORMAL;
            break;
        case thread_priority_high:
            windows_priority = THREAD_PRIORITY_ABOVE_NORMAL;
            break;
        case thread_priority_very_high:
            windows_priority = THREAD_PRIORITY_HIGHEST;
            break;
        default:
            windows_priority = THREAD_PRIORITY_NORMAL;
            break;
        }

        SetThreadPriority(handle, windows_priority);
    }
    
#if defined(_MSC_VER)
    static constexpr const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
    typedef struct tagTHREADNAME_INFO {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)

    typedef HRESULT (*set_thread_description_func)(HANDLE, PCWSTR);
    set_thread_description_func set_thread_description;

    bool thread_funcs_loaded = false;

    static void load_thread_funcs() {
        HMODULE kernlib = LoadLibraryW(L"kernel32");

        if (kernlib) {
            set_thread_description = reinterpret_cast<set_thread_description_func>(GetProcAddress(
                kernlib, "SetThreadDescription"));
        }

        thread_funcs_loaded = true;
    }

    static void convert_and_set_thread_name_win10(const char *thread_name) {
        const std::string thread_name_s(thread_name);
        const std::u16string thread_name_sw = utf8_to_ucs2(thread_name_s);
        set_thread_description(GetCurrentThread(), reinterpret_cast<PCWSTR>(thread_name_sw.c_str()));
    }

    void set_thread_name(const char *thread_name) {
        if (!thread_funcs_loaded) {
            load_thread_funcs();
        }

        // For Windows 10, there is SetThreadDescription... Like it.
        // Urosh is still using windows 7, so are many people.. Better keep it safe
        if (set_thread_description) {
            convert_and_set_thread_name_win10(thread_name);
        }

        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = thread_name;
        info.dwThreadID = GetCurrentThreadId();
        info.dwFlags = 0;

        __try {
            RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR *)&info);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
#endif
#else
    void set_thread_name(const char *thread_name) {
#if EKA2L1_PLATFORM(DARWIN)
        pthread_setname_np(thread_name);
#else
        pthread_setname_np(pthread_self(), thread_name);
#endif
    }
        
    void set_thread_priority(const thread_priority pri) {
        pthread_t this_thread = pthread_self();

        std::int32_t max_prio = sched_get_priority_max(SCHED_OTHER);
        std::int32_t min_prio = sched_get_priority_min(SCHED_OTHER);
        std::uint32_t level = static_cast<std::uint32_t>(pri) + 1;

        struct sched_param params;
        if (max_prio > min_prio) {
            params.sched_priority = min_prio + ((max_prio - min_prio) * level) / 4;
        } else {
            params.sched_priority = min_prio - ((min_prio - max_prio) * level) / 4;
        }

        pthread_setschedparam(this_thread, SCHED_OTHER, &params);
    }
#endif
}
