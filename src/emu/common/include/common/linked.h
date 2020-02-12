/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#pragma once

#include <common/algorithm.h>

#define E_LOFF(linked, object, linked_field) reinterpret_cast<object*>(reinterpret_cast<unsigned char*>(linked)   \
    - offsetof(object, linked_field))

namespace eka2l1::common {
    /**
     * \brief Represents a link in an one-way linked list
     */
    template <typename T>
    struct single_link {
        T *next { nullptr };
    };

    /**
     * \brief Represents a link in a double linked list, or queue.
     */
    template <typename T>
    struct double_link {
        T *previous { nullptr };
        T *next { nullptr };
    };

    struct double_linked_queue_element: public double_link<double_linked_queue_element> {
        void insert_before(double_linked_queue_element *this_guy) {
            previous = this_guy->previous;
            next = this_guy;

            this_guy->previous->next = this;
            this_guy->previous = this;
        }

        void insert_after(double_linked_queue_element *this_guy) {
            previous = this_guy;
            next = this_guy->next;

            this_guy->next->previous = this;
            this_guy->next = this;
        }

        double_linked_queue_element *deque() {
            next->previous = previous;
            previous->next = next;

            next = nullptr;
            previous = nullptr;

            return this;
        }

        bool alone() const {
            return next == previous;
        }
    };

    /**
     * \brief A double-linked queue.
     */
    struct roundabout {
        double_linked_queue_element elem_;

    public:
        explicit roundabout() {
            elem_.next = &elem_;
            elem_.previous = &elem_;
        }

        double_linked_queue_element *first() {
            return empty() ? nullptr : elem_.next;
        }

        double_linked_queue_element *last() {
            return empty() ? nullptr : elem_.previous;
        }

        double_linked_queue_element *end() {
            return empty() ? nullptr : &elem_;
        }

        void push(double_linked_queue_element *new_elem) {
            new_elem->previous = elem_.previous;
            new_elem->next = &elem_;

            elem_.previous->next = new_elem;
            elem_.previous = new_elem;
        }

        bool empty() const {
            return elem_.next == &elem_;
        }
    };

    template <size_t NUM>
    struct priority_roundabout_list {
        static constexpr std::size_t MASK_COUNT = (NUM + 31) >> 5;

        roundabout queues[NUM];
        std::uint32_t empty_mask[MASK_COUNT];

    public:
        explicit priority_roundabout_list() {
            std::fill(queues, queues + NUM, nullptr);
            std::fill(empty_mask, empty_mask + MASK_COUNT, 0);
        }

        bool add(const std::size_t priority, double_linked_queue_element *elem) {
            if (priority < NUM) {
                return false;
            }

            // Mark position to not be empty anymore!
            empty_mask[priority >> 5] |= 1 << (priority & 31);
            queues[priority].push(elem);

            return true;
        }

        double_linked_queue_element *highest() {  
            // Release code generation is corrupted somewhere on MSVC. Force fill is good so i guess it's the other.
            // Either way, until when i can repro this in a short code, files and bug got fixed, this stays here.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
            // Check the most significant bit and get the non-empty read queue
            for (std::uint32_t i = 0; i < NUM; i++) {
                int non_empty = common::count_leading_zero(empty_mask[0]);

                if (non_empty != 0) {
                    return queues[non_empty + i * 32];
                }
            }

            return nullptr;
#ifdef _MSC_VER
#pragma optimize("", on)
#endif
        }

        bool is_empty(const std::uint32_t pri) {
            return (empty_mask[pri >> 5] & (1 << (pri & 31)));
        }
    };
}