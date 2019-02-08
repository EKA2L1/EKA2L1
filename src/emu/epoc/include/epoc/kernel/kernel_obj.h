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

    namespace common {
        class chunkyseri;
    }

    using handle = std::uint32_t;

    namespace kernel {
        /*! \brief Ownership type for handle */
        enum class owner_type {
            process,
            thread,
            kernel
        };

        /*! \brief Access type for handle. */
        enum class access_type {
            //! Global access
            /*! Any process can share and access this handle. */
            global_access,

            //! Local access
            /*! Only the current process can access this handle. */
            local_access
        };

        /*! \brief HLE object type. */
        enum class object_type {
            thread,
            process,
            chunk,
            library,
            sema,
            mutex,
            timer,
            server,
            session,
            logical_device,
            physical_device,
            logical_channel,
            change_notifier,
            undertaker,
            msg_queue,
            prop
        };

        /*! \brief Base class for all kernel object. */
        class kernel_obj {
        protected:
            friend class kernel_system;

            //! The name of the object
            /*! Even local object will have a randomized name in here.
            */
            std::string obj_name;

            kernel_system *kern;

            access_type access;
            object_type obj_type;

            kernel_obj(kernel_system *kern, const std::string &obj_name,
                kernel::access_type access = access_type::local_access);

            int access_count = 0;

            uint32_t uid;

            kernel_obj(kernel_system *kern)
                : kern(kern) {
            }

        public:
            virtual ~kernel_obj() {}

            /*! \brief Get the name of the object.
             * \returns Object name.
            */
            std::string name() const {
                return obj_name;
            }

            /*! \brief Get the kernel system that own this object. 
                \returns The kernel system.
            */
            kernel_system *get_kernel_object_owner() const {
                return kern;
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

            void increase_access_count() { access_count++; }
            void decrease_access_count() { access_count--; };

            int get_access_count() { return access_count; }

            std::uint32_t unique_id() const {
                return uid;
            }

            /*! \brief Rename the kernel object. 
             * \param new_name The new name of object.
             */
            virtual void rename(const std::string &new_name) {
                obj_name = new_name;
            }

            virtual void do_state(common::chunkyseri &seri);
        };
    }
}
