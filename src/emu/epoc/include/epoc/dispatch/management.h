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

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace eka2l1::dispatch {
    template <typename T>
    struct object_manager {
    protected:
        std::vector<std::unique_ptr<T>> objs_;
        std::mutex lock_;

    public:
        using handle = std::uint32_t;

        /**
         * \brief   Add object to object list.
         * 
         * \param   obj  Unique pointer to move into the list.
         * 
         * \returns Handle to the object.
         */
        handle add_object(std::unique_ptr<T> &obj) {
            const std::lock_guard<std::mutex> guard(lock_);

            for (std::size_t i = 0; i < objs_.size(); i++) {
                if (objs_[i] == nullptr) {
                    objs_[i] = std::move(obj);
                    return static_cast<handle>(i + 1);
                }
            }

            objs_.push_back(std::move(obj));
            return static_cast<handle>(objs_.size());
        }

        /**
         * \brief       Remove the object correspond with given handle from object list.
         * 
         * \param       h Handle of the object.
         * 
         * \returns     False if handle is invalid or object can not be removed.
         */
        bool   remove_object(const handle h) {
            const std::lock_guard<std::mutex> guard(lock_);

            if (h == 0 || (h > objs_.size())) {
                return false;
            }

            objs_[h - 1].reset();
            return true;
        }

        /**
         * \brief       Get an object, given a handle.
         * \param       h Handle of the object.
         * 
         * \returns     Pointer to the object if handle is valid, else nullptr.
         */
        T*     get_object(const handle h) {
            const std::lock_guard<std::mutex> guard(lock_);

            if (h == 0 || (h > objs_.size())) {
                return nullptr;
            }

            return objs_[h - 1].get();
        }
    };
}