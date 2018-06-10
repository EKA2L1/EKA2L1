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

#include <cstdint>
#include <cstring>
#include <string>

namespace eka2l1 {
    namespace common {
        template <typename T>
        constexpr T max(T a, T b) {
            return a > b ? a : b;
        }

        template <typename T>
        constexpr T min(T a, T b) {
            return a > b ? b : a;
        }

        constexpr size_t KB(size_t kb) {
            return kb * 1024;
        }

        constexpr size_t MB(size_t mb) {
            return mb * KB(1024);
        }

        constexpr size_t GB(size_t gb) {
            return gb * MB(1024);
        }

        size_t find_nth(std::string targ, std::string str, size_t idx, size_t pos = 0);
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

        template <typename T>
        T next_power_of_two(const T target) {
            T power = static_cast<T>(std::log2l(static_cast<long double>(target)));
            return static_cast<T>(1 << power);
        }

        template <typename T>
        bool is_power_of_two(const T target) {
            T mask = 0;
            T power = static_cast<T>(std::log2l(static_cast<long double>(target)));

            for (T i = 0; i < power; ++i) {
                mask += static_cast<T>(1 << i);
            }

            return !(target & mask);
        }

        template <typename T>
        T align(T target, uint32_t alignment, int mode = 1) {
            if (alignment == 0) {
                return target;
            }

            uint32_t new_alignment = is_power_of_two(alignment) ? alignment : next_power_of_two(alignment);

            if (mode == 0) {
                return target - target % new_alignment;
            }

            return target + new_alignment - target % new_alignment;
        }
    }
}

