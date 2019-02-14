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

#pragma once

#include <common/types.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace eka2l1 {
    /*! \brief Contains functions that use frequently in the emulator */
    namespace common {
        /*
         * \brief Choose the greater variable 
		 *
		 * Compare two objects and choose the greater object to return.
		 */
        template <typename T>
        constexpr T max(T a, T b) {
            return a > b ? a : b;
        }

        /* 
         * \brief Choose the less variable
		 *
		 * Compare two objects, choose the less object to return.
		*/
        template <typename T>
        constexpr T min(T a, T b) {
            return a > b ? b : a;
        }

        /*! Convert KB to bytes */
        constexpr size_t KB(size_t kb) {
            return kb * 1024;
        }

        /*! Convert MB to bytes */
        constexpr size_t MB(size_t mb) {
            return mb * KB(1024);
        }

        /* Convert GB to bytes */
        constexpr size_t GB(size_t gb) {
            return gb * MB(1024);
        }

        /*! \brief Find the position of the Nth apperance of a string in another string
         *  \param idx The Nth apperance
		 *  \param pos The position to start looking for
		*/
        size_t find_nth(std::string targ, std::string str, size_t idx, size_t pos = 0);

        /*! Remove a string from another string if possible */
        void remove(std::string &inp, std::string to_remove);

        // https://stackoverflow.com/questions/12200486/how-to-remove-duplicates-from-unsorted-stdvector-while-keeping-the-original-or
        struct target_less {
            template <class It>
            bool operator()(It const &a, It const &b) const { return *a < *b; }
        };

        struct target_equal {
            template <class It>
            bool operator()(It const &a, It const &b) const { return *a == *b; }
        };

        /*! Erase all repeated object, but kept things in order. */
        template <class It>
        It uniquify(It begin, It const end) {
            std::vector<It> v;
            v.reserve(static_cast<size_t>(std::distance(begin, end)));
            for (It i = begin; i != end; ++i) {
                v.push_back(i);
            }
            std::sort(v.begin(), v.end(), target_less());
            v.erase(std::unique(v.begin(), v.end(), target_equal()), v.end());
            std::sort(v.begin(), v.end());
            size_t j = 0;
            for (It i = begin; i != end && j != v.size(); ++i) {
                if (i == v[j]) {
                    using std::iter_swap;
                    iter_swap(i, begin);
                    ++j;
                    ++begin;
                }
            }
            return begin;
        }

        /*! Get the next power of two of some number. */
        template <typename T>
        T next_power_of_two(const T target) {
            const std::uint8_t power = static_cast<std::uint8_t>(std::log2l(static_cast<long double>(target)));
            return static_cast<T>(1ULL << power);
        }

        /*! Check if the number is power of two. */
        template <typename T>
        bool is_power_of_two(const T target) {
            T mask = 0;
            T power = static_cast<T>(std::log2l(static_cast<long double>(target)));

            for (T i = 0; i < power; ++i) {
                mask += static_cast<T>(1 << i);
            }

            return !(target & mask);
        }

        /*! Do alignment */
        template <typename T>
        T align(T target, uint32_t alignment, int mode = 1) {
            if (alignment == 0) {
                return target;
            }

            uint32_t new_alignment = is_power_of_two(alignment) ? alignment : next_power_of_two(alignment);

            // No more align if it's already aligned
            if (target % new_alignment == 0) {
                return target;
            }

            if (mode == 0) {
                return target - target % new_alignment;
            }

            return target + new_alignment - target % new_alignment;
        }

        /**
         * \brief Compare two UTF2 string, ignoring it case
         * 
         * On Windows, this utilizes Win32 API CompareStringEx, on other platform it uses
         * std::towlower to lowercase two string and then compare.
         * 
         * \param s1 Left hand string.
         * \param s2 Right hand string.
         * 
         * \returns -1 if s1 < s2
         *           0 if s1 == s2
         *           1 if s1 > s2
         */
        int compare_ignore_case(const utf16_str &s1,
            const utf16_str &s2);

        /**
         * \brief Trim all space duplication to only one space between words
         * 
         * \returns A new string contains all space trimmed
         */
        std::string trim_spaces(std::string str);

        /**
         * \brief Replace all occurences of a word in a string with other word/string
         *
         * \param str           The string to find and replace
         * \param target        The word/string to find
         * \param replacement   The word/string to replace target
         * 
         * \returns A new string with all ocurrences of target replaced.
         */
        std::string replace_all(std::string str, const std::string &target, const std::string &replacement);
    }
}
