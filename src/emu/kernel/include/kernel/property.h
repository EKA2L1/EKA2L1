/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <common/queue.h>

#include <kernel/kernel_obj.h>
#include <mem/ptr.h>
#include <utils/reqsts.h>

#include <array>
#include <memory>
#include <vector>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    using thread_ptr = kernel::thread *;

    namespace service {
        enum class property_type {
            int_data,
            bin_data,
            unk
        };

        /*! \brief Property is a kind of environment data. 
		 *
		 * Property is defined by cagetory and key. Each property contains either
		 * integer or binary data. Properties are stored in the kernel until shutdown,
		 * and they are the way of ITC (Inter-Thread communication).
 		 *
		*/
        class property : public kernel::kernel_obj, public std::pair<int, int> {
        public:
            typedef void (*data_change_callback_handler)(void *userdata, service::property *prop);

        protected:
            union {
                int ndata;
                std::array<uint8_t, 512> bindata;
            } data;

            uint32_t data_len;
            uint32_t bin_data_len;

            service::property_type data_type;

            threadsafe_cn_queue<epoc::notify_info *> subscription_queue;

            using data_change_callback = std::pair<void *, data_change_callback_handler>;
            std::vector<data_change_callback> data_change_callbacks;

            void fire_data_change_callbacks();

        public:
            explicit property(kernel_system *kern);

            /**
             * \brief Add a callback that gets waken up when data changed.
             * 
             * Note: This does not lock the kernel.
             */
            void add_data_change_callback(void *userdata, data_change_callback_handler handler);

            void define(service::property_type pt, uint32_t pre_allocated);

            /**
             * \brief Set the property value (integer).
             *
             * If the property type is not integer, this return false, else
             * it will set the value and notify the request.		
             *
             * \param val The value to set.
			*/
            bool set_int(int val);

            /**
             * \brief Set the property value (bin).
             *
             * If the property type is not binary, this return false, else
             * it will set the value and notify the request.	
             * 
             * \param data The pointer to binary data.	
             * \param arr_length The binary data length.
             *
             * \returns false if arr_length exceeds allocated data length or property is not binary.			 
			*/
            bool set(uint8_t *data, uint32_t arr_length);

            template <typename T>
            bool set(T data) {
                return set(reinterpret_cast<uint8_t *>(&data), sizeof(T));
            }

            int get_int();
            std::vector<uint8_t> get_bin();

            template <typename T>
            std::optional<T> get_pkg() {
                auto bin = get_bin();

                if (bin.size() != sizeof(T)) {
                    return std::optional<T>{};
                }

                T ret;
                memcpy(&ret, bin.data(), bin.size());

                return ret;
            }

            void subscribe(epoc::notify_info &info);
            bool cancel(const epoc::notify_info &info);

            /*! \brief Notify the request that there is data change */
            void notify_request(const std::int32_t err);
        };

        struct property_reference : public kernel::kernel_obj {
            property *prop_;
            epoc::notify_info nof_;

        public:
            explicit property_reference(kernel_system *kern, property *prop);

            /**
             * \brief       Get the property kernel object.
             * \returns     The property object.
             */
            property *get_property_object() {
                return prop_;
            }

            /**
             * \brief   Subscribe to property change.
             * \param   info The info of the subscribe request.
             * 
             * \returns True if no pending notify request is attached to this reference.
             */
            bool subscribe(const epoc::notify_info &info);

            /**
             * \brief    Cancel a subscription.
             * \returns  True on success and pending notify request is not empty.
             */
            bool cancel();
        };
    }
}
