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
        using addition_callback_func = std::function<void(const std::size_t)>;
        using remove_callback_func = addition_callback_func;

        template <typename T>
        using compare_func = std::function<int(const T &lhs, const T &rhs)>;

        template <typename T>
        int default_compare_func(const T &lhs, const T &rhs) {
            if (lhs < rhs) {
                return -1;
            } else if (lhs == rhs) {
                return 0;
            }

            return 1;
        }

        /**
         * \brief Detect changes between two iterable-objects, allow callback to handle
         *        addition and remove.
         * 
         * Given two iterable object: maybe an old and new version of a vector, this function
         * use binary search to detect changes between them.
         * 
         * Changes mentioned here are addition and removal. The function will take two callback function:
         * - The first function will handle addition. There is only one argument gonna be passed for this function,
         *   it's the index of the added object in the second iterable object.
         * - The second function will handle remove. Same as addition, the only argument is the index of the removed
         *   object in the first iterable object.
         * 
         * This function can handle any iterable objects that support these requirements:
         * - Have at operator []
         * - Have a function named size(), which will returns an integer.
         * 
         * Requires two iterable object to be sorted.
         * 
         * \param old_           The first iterable object.
         * \param new_           The second iterable object.
         * \param add_callback_  Callback when detect a new element is added to the second object.
         * \param rev_callback_  Callback when detect an element is removed from the first object.
         * \param compare_       Function comparing two element of the iterable object.
         */
        template <typename T, typename H = typename T::value_type>
        void detect_changes(const T &old_, const T &new_, addition_callback_func add_callback_, 
            remove_callback_func rev_callback_, compare_func<H> compare_ = default_compare_func<H>) {
            std::size_t index_new = 0;
            std::size_t index_old = 0;

            const std::size_t size_new = new_.size();
            const std::size_t size_old = old_.size();

            while (index_old < size_old || index_new < size_new) {
                if (index_old == size_old) {
                    // At the end of old list, any elements left in new vector should be addition
                    add_callback_(index_new);
                    index_new++;
                } else if (index_new == size_new) {
                    // At the end of new list, any elements left in old vector should be removed
                    rev_callback_(index_old);
                    index_old++;
                } else {
                    int comp_result = compare_(old_[index_old], new_[index_new]);

                    switch (comp_result) {
                    case 0: {
                        index_new++;
                        index_old++;

                        break;
                    }

                    case -1: {
                        rev_callback_(index_old);
                        index_old++;

                        break;
                    }

                    default: {
                        add_callback_(index_new);
                        index_new++;

                        break;
                    }
                    }
                }
            }
        }

        template <typename T>
        bool default_equal_comparator(const T &lhs, const T &rhs) {
            return lhs == rhs;
        }

        /**
         * \brief Merge a iterable object to other one and replaces duplicate elements.
         * \param target The iterable object to be inserted to.
         * \param to_append The iterable object to have its element being inserted to.
         * \param comparator The equal comparator.
         */
        template <typename T, typename F>
        void merge_and_replace(T &target, const T &to_append, F &comparator = default_equal_comparator<typename T::value_type>) {
            for (auto i = to_append.begin(); i != to_append.end(); i++) {
                for (auto j = target.begin(); j != target.end(); j++) {
                    if (comparator(*i, *j)) {
                        target.erase(j);
                    }
                }
            }

            target.insert(target.end(), to_append.begin(), to_append.end());
        }

        template <typename T, typename F>
        void erase_elements(T &target, F &condition) {
            auto it = target.begin();

            while (it != target.end()) {
                if (condition(*it)) {
                    it = target.erase(it);
                } else {
                    ++it;
                }
            }
        }

        /**
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

        /*! Get the next power of two of some number. */
        template <typename T>
        T next_power_of_two(const T target) {
            const std::uint8_t power = static_cast<std::uint8_t>(std::log2l(static_cast<long double>(target)));
            return static_cast<T>(1ULL << (power + 1));
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

        /**
         * \brief Lowercase the string.
         * 
         * \returns String lowercased.
         */
        std::string lowercase_string(std::string str);

        /**
         * \brief Lowercase UCS2 string.
         * 
         * \returns String lowercased.
         */
        std::u16string lowercase_ucs2_string(std::u16string str);

        /**
         *  \brief Returns if the platform is case-senstive or not
         */
        bool is_platform_case_sensitive();
    }
}
