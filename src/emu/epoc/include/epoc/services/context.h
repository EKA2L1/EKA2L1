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

#include <common/algorithm.h>
#include <common/log.h>

#include <epoc/ipc.h>
#include <epoc/ptr.h>

#include <cstring>
#include <optional>
#include <string>

namespace eka2l1 {
    class system;

    namespace epoc {
        struct security_info;
        struct security_policy;
    }

    namespace service {
        /*! \brief Context used to pass to IPC function */
        struct ipc_context {
            eka2l1::system *sys;
            ipc_msg_ptr msg;

            bool signaled = false;

            template <typename T>
            std::optional<T> get_arg(int idx);

            template <typename T>
            std::optional<T> get_arg_packed(int idx) {
                T ret;

                std::optional<std::string> dat = get_arg<std::string>(idx);

                if (!dat) {
                    return std::optional<T>{};
                }

                if (dat->length() > sizeof(T)) {
                    LOG_WARN("Size of data provided in descriptor index is larger than size of data to get ({} vs {})",
                        dat->length(), sizeof(T));
                }

                memcpy(&ret, dat->data(), common::min(sizeof(T), dat->length()));

                return std::make_optional<T>(std::move(ret));
            }

            void set_request_status(int res);

            int flag() const;

            /*
             * \brief Get a pointer to the data of an IPC descriptor argument
            */
            std::uint8_t *get_arg_ptr(int idx);

            /**
             * \brief Set length of a descriptor passed as IPC argument in given index.
             * 
             * \returns True if success.
             */
            bool set_arg_des_len(const int idx, const std::uint32_t len);

            bool write_arg(int idx, uint32_t data);
            bool write_arg(int idx, const std::u16string &data);

            bool write_arg_pkg(int idx, uint8_t *data, uint32_t len, int *err_code = nullptr);

            // Package an argument, write it to a destination
            template <typename T>
            bool write_arg_pkg(int idx, T data) {
                return write_arg_pkg(idx, reinterpret_cast<uint8_t *>(&data), sizeof(T));
            }

            bool satisfy(epoc::security_policy &policy, epoc::security_info *missing = nullptr);
        };
    }
}