/*
* Copyright (c) 2019 EKA2L1 Team.
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

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1::epoc {
    class object_container;

    struct ref_count_object {
        std::uint32_t id;

        int count{ 0 };
        std::string name;

        object_container *owner;

        ref_count_object()
            : owner(nullptr)
            , count(0) {
        }

        virtual ~ref_count_object();

        virtual void ref();
        virtual void deref();
    };

    class object_container {
    public:
        virtual ~object_container() {}

        virtual void clear() = 0;
        virtual bool remove(ref_count_object *obj) = 0;
    };

    using handle = std::uint32_t;

    class object_table {
        std::atomic<std::uint32_t> next_instance;
        std::vector<ref_count_object *> objects;

    public:
        explicit object_table();

        handle add(ref_count_object *obj);
        bool remove(handle obj_handle);

        ref_count_object *get_raw(handle obj_handle);

        template <typename T>
        T *get(handle obj_handle) {
            return reinterpret_cast<T *>(get_raw(obj_handle));
        }

        ref_count_object *operator[](handle obj_handle) {
            return get_raw(obj_handle);
        }

        decltype(objects)::iterator begin() {
            return objects.begin();
        }

        decltype(objects)::iterator end() {
            return objects.end();
        }
    };
}
