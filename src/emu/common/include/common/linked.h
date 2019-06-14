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

#define LOFF(linked, object, linked_field) reinterpret_cast<object*>(reinterpret_cast<unsigned char*>(linked)   \
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

        void deque() {
            next->previous = previous;
            previous->next = next;

            next = nullptr;
            previous = nullptr;
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
            return elem_.next;
        }

        double_linked_queue_element *last() {
            return elem_.previous;
        }

        void push(double_linked_queue_element *new_elem) {
            new_elem->previous = elem_.previous;
            new_elem->next = &elem_;

            elem_.previous->next = new_elem;
            elem_.previous = new_elem;
        }
    };
}