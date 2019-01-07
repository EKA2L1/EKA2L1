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

#include <epoc/kernel/kernel_obj.h>
#include <epoc/ptr.h>
#include <epoc/utils/reqsts.h>

#include <array>
#include <memory>
#include <vector>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;

    namespace service {
        enum class property_type {
            int_data,
            bin_data
        };

        /*! \brief Property is a kind of environment data. 
		 *
		 * Property is defined by cagetory and key. Each property contains either
		 * integer or binary data. Properties are stored in the kernel until shutdown,
		 * and they are the way of ITC (Inter-Thread communication).
 		 *
		*/
        class property : public kernel::kernel_obj, public std::pair<int, int> {
        protected:
            union {
                int ndata;
                std::array<uint8_t, 512> bindata;
            } data;

            uint32_t data_len;
            uint32_t bin_data_len;

            service::property_type data_type;

            struct request {
                thread_ptr request_thr;
                epoc::request_status *request_status = 0;
            } subscribe_request;

        public:
            property(kernel_system *kern);

            void define(service::property_type pt, uint32_t pre_allocated);

            /*! \brief Set the property value (integer).
             *
             * If the property type is not integer, this return false, else
             * it will set the value and notify the request.		
             *
             * \param val The value to set.		 
			*/
            bool set_int(int val);

            /*! \brief Set the property value (bin).
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

            void subscribe(epoc::request_status *sts);

            void cancel();

            /*! \brief Notify the request that there is data change */
            void notify_request();
        };
    }
}