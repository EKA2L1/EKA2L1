/*
 * Copyright (c) 2020 EKA2L1 Team.
 * Copyright (c) 2018 yuzu emulator team
 * 
 * This file is part of EKA2L1 project.
 * 
 * ring_buffer class is a part of the Yuzu emulator project, originally licensed
 * under GPLv2 license.
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
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

namespace eka2l1::common {
    template <typename T>
    bool default_data_free_check_func(T &data) {
        return !data;
    }

    template <typename T>
    void default_data_free_func(T &data) {
        data = 0;
    }

    template <typename T>
    class identity_container {
    public:
        using data_free_check_func = std::function<bool(T &)>;
        using data_free_func = std::function<void(T &)>;

    protected:
        std::vector<T> data_;
        data_free_check_func check_;
        data_free_func freer_;

    public:
        struct iterator {
            typename std::vector<T>::iterator ite_;
            identity_container *container_;

            typedef std::size_t difference_type;
            typedef T value_type;
            typedef T* pointer;
            typedef T& reference;
            typedef std::forward_iterator_tag iterator_category;

            iterator(identity_container *container, typename std::vector<T>::iterator ite)
                : ite_(ite)
                , container_(container) {
            }

            iterator &operator++() {
                do {
                    ite_++;
                } while ((ite_ != container_->data_.end()) && (container_->check_(*ite_)));

                return *this;
            }

            bool operator!=(const iterator &ite) {
                return ite_ != ite.ite_;
            }

            T &operator*() {
                return *ite_;
            }
        };

    public:
        explicit identity_container(data_free_check_func check_func = default_data_free_check_func<T>,
            data_free_func freer = default_data_free_func<T>)
            : check_(check_func)
            , freer_(freer) {
        }

        iterator begin() {
            return iterator(this, data_.begin());
        }

        iterator end() {
            return iterator(this, data_.end());
        }

        std::size_t add(T &elem) {
            for (std::size_t i = 0; i < data_.size(); i++) {
                if (check_(data_[i])) {
                    data_[i] = std::move(elem);
                    return i + 1;
                }
            }

            data_.push_back(std::move(elem));
            return data_.size();
        }

        bool remove(const std::size_t elem_id) {
            if ((elem_id == 0) || (elem_id > data_.size())) {
                return false;
            }

            freer_(data_[elem_id - 1]);
            return true;
        }

        T *get(const std::size_t elem_id) {
            if ((elem_id == 0) || (elem_id > data_.size())) {
                return nullptr;
            }

            return &data_[elem_id - 1];
        }
    };
        
    /// SPSC ring buffer
    /// @tparam T            Element type
    /// @tparam capacity     Number of slots in ring buffer
    template <typename T, std::size_t capacity_>
    class ring_buffer {
        /// A "slot" is made of a single `T`.
        static constexpr std::size_t slot_size = sizeof(T);
        // T must be safely memcpy-able and have a trivial default constructor.
        static_assert(std::is_trivial_v<T>);
        // Ensure capacity is sensible.
        static_assert(capacity_ < std::numeric_limits<std::size_t>::max() / 2);
        static_assert((capacity_ & (capacity_ - 1)) == 0, "capacity must be a power of two");
        // Ensure lock-free.
        static_assert(std::atomic_size_t::is_always_lock_free);

    public:
        /// Pushes slots into the ring buffer
        /// @param new_slots   Pointer to the slots to push
        /// @param slot_count  Number of slots to push
        /// @returns The number of slots actually pushed
        std::size_t push(const void* new_slots, std::size_t slot_count) {
            const std::size_t write_index = write_index_.load();
            const std::size_t slots_free = capacity_ + read_index_.load() - write_index;
            const std::size_t push_count = std::min(slot_count, slots_free);

            const std::size_t pos = write_index % capacity_;
            const std::size_t first_copy = std::min(capacity_ - pos, push_count);
            const std::size_t second_copy = push_count - first_copy;

            const char* in = static_cast<const char*>(new_slots);
            std::memcpy(data_.data() + pos, in, first_copy * slot_size);
            in += first_copy * slot_size;
            std::memcpy(data_.data(), in, second_copy * slot_size);

            write_index_.store(write_index + push_count);

            return push_count;
        }

        std::size_t push(const std::vector<T>& input) {
            return push(input.data(), input.size());
        }

        /// Pops slots from the ring buffer
        /// @param output     Where to store the popped slots
        /// @param max_slots  Maximum number of slots to pop
        /// @returns The number of slots actually popped
        std::size_t pop(void* output, std::size_t max_slots = ~std::size_t(0)) {
            const std::size_t read_index = read_index_.load();
            const std::size_t slots_filled = write_index_.load() - read_index;
            const std::size_t pop_count = std::min(slots_filled, max_slots);

            const std::size_t pos = read_index % capacity_;
            const std::size_t first_copy = std::min(capacity_ - pos, pop_count);
            const std::size_t second_copy = pop_count - first_copy;

            char* out = static_cast<char*>(output);
            std::memcpy(out, data_.data() + pos, first_copy * slot_size);
            out += first_copy * slot_size;
            std::memcpy(out, data_.data(), second_copy * slot_size);

            read_index_.store(read_index + pop_count);

            return pop_count;
        }

        std::vector<T> pop(std::size_t max_slots = ~std::size_t(0)) {
            std::vector<T> out(std::min(max_slots, capacity_));
            const std::size_t count = pop(out.data(), out.size());
            out.resize(count);
            return out;
        }

        void reset() {
            read_index_ = 0;
            write_index_ = 0;
        }

        /// @returns Number of slots used
        [[nodiscard]] std::size_t size() const {
            return write_index_.load() - read_index_.load();
        }

        /// @returns Maximum size of ring buffer
        [[nodiscard]] constexpr std::size_t capacity() const {
            return capacity_;
        }

    private:
        // It is important to align the below variables for performance reasons:
        // Having them on the same cache-line would result in false-sharing between them.
        // TODO: Remove this ifdef whenever clang and GCC support
        //       std::hardware_destructive_interference_size.
    #if defined(_MSC_VER) && _MSC_VER >= 1911
        alignas(std::hardware_destructive_interference_size) std::atomic_size_t read_index_{0};
        alignas(std::hardware_destructive_interference_size) std::atomic_size_t write_index_{0};
    #else
        alignas(128) std::atomic_size_t read_index_{0};
        alignas(128) std::atomic_size_t write_index_{0};
    #endif

        std::array<T, capacity_> data_;
    };
}