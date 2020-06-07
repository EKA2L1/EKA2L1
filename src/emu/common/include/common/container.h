/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <functional>
#include <vector>

namespace eka2l1::common {
    template <typename T>
    bool default_data_free_check_func(T &data) {
        return !data;
    }

    template <typename T>
    void default_data_free_func(T &data) {
        data = nullptr;
    }

    template <typename T>
    class identity_container {
    public:
        using data_free_check_func = std::function<bool(T&)>;
        using data_free_func = std::function<void(T&)>;

    protected:
        std::vector<T> data_;
        data_free_check_func check_;
        data_free_func freer_;

    public:
        struct iterator {
            typename std::vector<T>::iterator ite_;
            identity_container *container_;

            iterator(identity_container *container, typename std::vector<T>::iterator ite)
                : ite_(ite)
                , container_(container) {
            }

            iterator& operator ++ () {
                do {
                    ite_++;
                } while ((ite_ != container_->data_.end()) && (container_->check_(*ite_)));

                return *this;
            }

            bool operator != (const iterator &ite) {
                return ite_ != ite.ite_;
            }

            T &operator *() {
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
}