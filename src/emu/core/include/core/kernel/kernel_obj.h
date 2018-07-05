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

#include <cstdint>
#include <string>

namespace eka2l1 {
    class kernel_system;

    namespace kernel {
        using uid = uint64_t;

        enum class owner_type {
            kernel,  // Kernel has id of 0xDDDDDDDD
            process,
            thread
        };

        enum class access_type {
            global_access,
            local_access
        };

        enum class object_type {
            thread,
            sema,
            mutex,
            timer,
            chunk
        };

        /*! \brief Base class for all kernel object. */
        class kernel_obj {
        protected:
            std::string obj_name;
            uid obj_id;

            bool obj_user_closeable = true;

            kernel_system *kern;
            
            // If the owner_type is process, this will contains the process index in the kernel
            // Else, it will contains the thread's id
            kernel::uid owner;
            kernel::owner_type owner_type;

            access_type access;
            object_type obj_type;

            kernel_obj(kernel_system *kern, const std::string &obj_name, kernel::owner_type owner_type, kernel::uid owner, 
                kernel::access_type access = access_type::local_access);

            kernel_obj(kernel_system *kern, const uid obj_id, const std::string &obj_name, kernel::owner_type owner_type, kernel::uid owner, 
                kernel::access_type access = access_type::local_access)
                : obj_id(obj_id)
                , obj_name(obj_name)
                , kern(kern)
                , owner(owner)
                , owner_type(owner_type) 
                , access(access) {}

        public:
            std::string name() const {
                return obj_name;
            }

            uid unique_id() const {
                return obj_id;
            }

            kernel_system *get_kernel_object_owner() const {
                return kern;
            }

            bool user_closeable() const {
                return obj_user_closeable;
            }

            void user_closeable(bool opt) {
                obj_user_closeable = opt;
            }

            kernel::uid obj_owner() const {
                return owner;
            }

            kernel::owner_type get_owner_type() const {
                return owner_type;
            }

            kernel::access_type get_access_type() const {
                return access;
            }

            void set_access_type(kernel::access_type acc) {
                access = acc;
            }

            object_type get_object_type() const {
                return obj_type;
            }

            void set_owner_type(kernel::owner_type new_owner) {
                owner_type = new_owner;
            }

            void rename(const std::string &new_name) {
                obj_name = new_name;
            }
        };
    }
}

