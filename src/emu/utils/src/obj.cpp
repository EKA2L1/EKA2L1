/*
* Copyright (c) 2019 EKA2L1 Team.
*
* This file is part of EKA2L1 project
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

#include <cassert>
#include <common/log.h>
#include <utils/obj.h>

namespace eka2l1::epoc {
    object_table::object_table()
        : next_instance(1) {
    }

    static handle make_handle(const std::uint32_t idx, const std::uint32_t inst) {
        return static_cast<handle>((inst << 16) | (idx));
    }

    handle object_table::add(ref_count_object *obj) {
        for (std::size_t i = 0; i < objects.size(); i++) {
            if (!objects[i]) {
                objects[i] = obj;
                obj->ref();

                next_instance = (next_instance + 1) & 0xFFFF;
                return make_handle(static_cast<std::uint32_t>(i + 1), next_instance);
            }
        }

        objects.push_back(obj);
        obj->ref();

        return make_handle(static_cast<std::uint32_t>(objects.size()), next_instance++);
    }

    bool object_table::remove(handle obj_handle) {
        const std::size_t idx = obj_handle & 0xFFFF;
        if (idx > objects.size() || idx == 0) {
            return false;
        }

        objects[idx - 1]->deref();
        objects[idx - 1] = nullptr;
        return true;
    }

    ref_count_object *object_table::get_raw(handle obj_handle) {
        const std::size_t idx = obj_handle & 0xFFFF;
        if (idx > objects.size() || idx == 0) {
            return nullptr;
        }

        return objects[idx - 1];
    }

    void ref_count_object::ref() {
        count++;
    }

    void ref_count_object::deref() {
        assert(count > 0 && "Reference count must be greater than 0 when dereferencing!");
        count--;

        if (count == 0) {
            owner->remove(this);
        }
    }

    ref_count_object::~ref_count_object() {
    }
}
