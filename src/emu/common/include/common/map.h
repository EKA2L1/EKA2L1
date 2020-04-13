/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
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

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>

namespace eka2l1::common {
    template <typename T>
    std::enable_if_t<std::is_integral_v<T>, bool> is_vector_static_map_entry_empty(const T &key) {
        return key == 0;
    }

    template <typename T>
    std::enable_if_t<std::is_integral_v<T>, void> mark_vector_static_map_entry_empty(T &key) {
        key = 0;
    }

    template <typename K, typename V, size_t LEN,
        typename IS_EMPTY_FUNC_TYPE = decltype(is_vector_static_map_entry_empty<K>),
        typename MARK_EMPTY_FUNC_TYPE = decltype(mark_vector_static_map_entry_empty<K>)>
    class vector_static_map {
        K keys[LEN];
        V values[LEN];

        std::function<bool(const K &)> is_empty_func;
        std::function<void(K &)> mark_empty_func;

    public:
        explicit vector_static_map(IS_EMPTY_FUNC_TYPE is_empty_func = is_vector_static_map_entry_empty<K>,
            MARK_EMPTY_FUNC_TYPE mark_empty_func = mark_vector_static_map_entry_empty<K>)
            : is_empty_func(is_empty_func)
            , mark_empty_func(mark_empty_func) {
            clear();
        }

        void clear() {
            std::memset(keys, 0, sizeof(K) * LEN);
            std::memset(values, 0, sizeof(V) * LEN);
        }

        void init(IS_EMPTY_FUNC_TYPE a_is_empty_func = is_vector_static_map_entry_empty<K>,
            MARK_EMPTY_FUNC_TYPE a_mark_empty_func = mark_vector_static_map_entry_empty<K>) {
            is_empty_func = a_is_empty_func;
            mark_empty_func = a_mark_empty_func;

            clear();
        }

        constexpr std::size_t count() const {
            return LEN;
        }

        bool add(const K &key, const V &value) {
            for (std::size_t i = 0; i < LEN; i++) {
                if (is_empty_func(keys[i])) {
                    keys[i] = std::move(key);
                    values[i] = std::move(value);

                    return true;
                }
            }

            return false;
        }

        bool remove(const K &key) {
            for (std::size_t i = 0; i < LEN; i++) {
                if (keys[i] == key) {
                    mark_empty_func(keys[i]);
                    return true;
                }
            }

            return false;
        }

        V *find(const K &key) {
            for (std::size_t i = 0; i < LEN; i++) {
                if (keys[i] == key) {
                    return &values[i];
                }
            }

            return nullptr;
        }
    };
}
