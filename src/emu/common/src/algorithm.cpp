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

#include <common/algorithm.h>
#include <common/platform.h>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <cctype>
#include <cwctype>

namespace eka2l1 {
    namespace common {
        bool is_platform_case_sensitive() {
#if EKA2L1_PLATFORM(WIN32)
            return false;
#else
            return true;
#endif
        }

        size_t find_nth(std::string targ, std::string str, size_t idx, size_t pos) {
            size_t found_pos = targ.find(str, pos);

            if (1 == idx || found_pos == std::string::npos) {
                return found_pos;
            }

            return find_nth(targ, str, idx - 1, found_pos + 1);
        }

        void remove(std::string &inp, std::string to_remove) {
            size_t pos = 0;

            do {
                pos = inp.find(to_remove, pos);

                if (pos == std::string::npos) {
                    break;
                } else {
                    inp.erase(pos, to_remove.length());
                }
            } while (true);
        }

        int compare_ignore_case(const utf16_str &s1,
            const utf16_str &s2) {
#if EKA2L1_PLATFORM(WIN32)
            // According to the MSDN documentation, the CompareStringEx function
            // is optimized for NORM_IGNORECASE and string lengths specified as -1.

            const int result = CompareStringEx(
                LOCALE_NAME_INVARIANT,
                NORM_IGNORECASE,
                reinterpret_cast<const wchar_t *>(s1.c_str()),
                -1,
                reinterpret_cast<const wchar_t *>(s2.c_str()),
                -1,
                nullptr, // reserved
                nullptr, // reserved
                0 // reserved
            );

            switch (result) {
            case CSTR_EQUAL:
                return 0;

            case CSTR_GREATER_THAN:
                return 1;

            case CSTR_LESS_THAN:
                return -1;

            default:
                return -2;
            }
#else
            for (size_t i = 0; i < common::min<std::size_t>(s1.size(), s2.size()); i++) {
                const wchar_t t1 = towlower(s1[i]);
                const wchar_t t2 = towlower(s2[i]);

                if (t1 > t2) {
                    return 1;
                } else if (t1 < t2) {
                    return -1;
                }
            }

            if (s1.size() == s2.size())
                return 0;

            if (s1.size() > s2.size()) {
                return 1;
            }

            return -1;
#endif
        }

        int compare_ignore_case(const char *s1,
            const char *s2) {
#if EKA2L1_PLATFORM(WIN32)
            // According to the MSDN documentation, the CompareStringEx function
            // is optimized for NORM_IGNORECASE and string lengths specified as -1.

            const int result = CompareStringA(
                LOCALE_INVARIANT,
                NORM_IGNORECASE,
                s1,
                -1,
                s2,
                -1);

            switch (result) {
            case CSTR_EQUAL:
                return 0;

            case CSTR_GREATER_THAN:
                return 1;

            case CSTR_LESS_THAN:
                return -1;

            default:
                return -2;
            }
#else
            size_t s1_size = strlen(s1);
            size_t s2_size = strlen(s2);

            for (size_t i = 0; i < common::min<std::size_t>(s1_size, s2_size); i++) {
                const char t1 = tolower(s1[i]);
                const char t2 = tolower(s2[i]);

                if (t1 > t2) {
                    return 1;
                } else if (t1 < t2) {
                    return -1;
                }
            }

            if (s1_size == s2_size)
                return 0;

            if (s1_size > s2_size) {
                return 1;
            }

            return -1;
#endif
        }

        std::string lowercase_string(std::string str) {
            // TODO: Better try
            std::transform(str.begin(), str.end(), str.begin(),
                [](const char c) -> unsigned char { return std::tolower(c); });

            return str;
        }

        std::string uppercase_string(std::string str) {
            std::transform(str.begin(), str.end(), str.begin(),
                [](const char c) -> unsigned char { return std::toupper(c); });

            return str;
        }

        std::u16string lowercase_ucs2_string(std::u16string str) {
            // TODO: Better try
            std::transform(str.begin(), str.end(), str.begin(),
                [](const char16_t c) -> char16_t { return std::towlower(c); });

            return str;
        }

        int count_leading_zero(const std::uint32_t v) {
#if defined(__GNUC__) || defined(__clang__)
            return __builtin_clz(v);
#elif defined(_MSC_VER)
            DWORD lz = 0;
            _BitScanReverse(&lz, v);

            if (lz != 0)
                return static_cast<int>(31 - lz);

            return 32;
#endif
        }
        
        int find_least_significant_bit_one(const std::uint64_t v) {
#if defined(__GNUC__) || defined(__clang__)
            int res = __builtin_ffsll(static_cast<long long>(v));
            return (res <= 0) ? 64 : (res - 1);
#elif defined(_MSC_VER)
            DWORD lz = 64;
            _BitScanForward64(&lz, v);

            return lz;
#endif
        }

        int find_most_significant_bit_one(const std::uint32_t v) {
            return 32 - count_leading_zero(v);
        }

        static std::uint64_t inaccurate_multiply_and_divide_qwords(std::uint64_t m1, std::uint64_t m2, std::uint64_t d1) {
            return static_cast<std::uint64_t>(m1 * (static_cast<double>(m2) / d1));
        }

        std::uint64_t multiply_and_divide_qwords(std::uint64_t m1, std::uint64_t m2, std::uint64_t d1) {
#if (EKA2L1_ARCH(X64) || (EKA2L1_ARCH(ARM64)))
#if defined(__GNUC__) || defined(__clang__)
            using gcc_uint128_t = unsigned __int128;
            return static_cast<std::uint64_t>((static_cast<gcc_uint128_t>(m1) * m2) / d1);
#elif defined(_MSC_VER)
            std::uint64_t low = 0;
            std::uint64_t high = 0;

            low = _umul128(m1, m2, &high);

            // Older version of MSVC dont follow the underscore naming
            std::uint64_t remainder = 0;

#if _MSC_VER < 1923
            return udiv128(high, low, d1, &remainder);
#else
            return _udiv128(high, low, d1, &remainder);
#endif

#else
            return inaccurate_multiply_and_divide_qwords(m1, m2, d1);
#endif
#else
            return inaccurate_multiply_and_divide_qwords(m1, m2, d1);
#endif
        }

#if defined(_MSC_VER)
        bool cpu_support_checked = false;
        bool cpu_support_popcnt = false;

        void check_cpu_features() {
            if (cpu_support_checked) {
                return;
            }

            int info[4];
            __cpuid(info, 1);

            if (static_cast<std::uint32_t>(info[2]) & (1 << 23)) {
                cpu_support_popcnt = true;
            } else {
                cpu_support_popcnt = false;
            }

            cpu_support_checked = true;
        }
#endif

        int count_bit_set(const std::uint32_t v) {
#if defined(__GNUC__) || defined(__clang__)
            return __builtin_popcount(v);
#else
            check_cpu_features();

            if (cpu_support_popcnt) {
                return static_cast<int>(__popcnt(v));
            } else {
                // From GCC
                std::uint32_t i = v;

                i = i - ((i >> 1) & 0x55555555);
                i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
                return (((i + (i >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
            }
#endif
        }
    }
}
