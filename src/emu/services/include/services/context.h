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

#include <kernel/ipc.h>
#include <mem/ptr.h>

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
        /**
         * \brief Context struct, wrapping around IPC message object.
         * 
         * This struct provide utilities, helping with the HLE of services, by wrapping around the IPC
         * message kernel object. Client can read/write descriptor data, and get packed structure.
         * 
         * The final thing in every IPC request is to set the request status, which is also what this
         * struct supported.
         */
        struct ipc_context {
            explicit ipc_context(const bool auto_free = true, const bool accurate_timing = false);
            ~ipc_context();

            eka2l1::system *sys; ///< The system instance pointer.
            ipc_msg_ptr msg; ///< The IPC message that this struct wrapped.

            bool signaled = false; ///< A safe-check if a request status is set. This allow setting multiple
                ///< time with only one time it signaled the client.

            bool auto_free = false; ///< Auto free this message when the context is destroyed. Useful
                ///< for HLE context.

            bool accurate_timing = false;

            /**
             * \brief   Get raw IPC argument value.
             * 
             * Accept template include: std::string, std::u16string, and integer types.
             * 
             * \param   idx The index of the IPC argument.
             * \returns The raw data asked, if available. Else std::nullopt.
             * 
             * \sa      get_argument_data_from_descriptor
             */
            template <typename T>
            std::optional<T> get_argument_value(const int idx);

            /**
             * \brief    Convert descriptor data to a struct.
             * 
             * \param    idx The index of the IPC argument.
             * \returns  std::nullopt if argument is not a descriptor, or index is out of range.
             *           Else, returns the target struct data.
             * 
             * \sa       get_argument_data_from_descriptor
             */
            template <typename T>
            std::optional<T> get_argument_data_from_descriptor(const int idx) {
                std::uint8_t *data = get_descriptor_argument_ptr(idx);

                if (!data) {
                    return std::nullopt;
                }

                const std::size_t packed_size = get_argument_data_size(idx);

                if (packed_size != sizeof(T)) {
                    LOG_WARN(SERVICE_TRACK, "Getting packed struct with mismatch size ({} vs {}), size to get "
                             "will be automatically clamped",
                        packed_size, sizeof(T));
                }

                T object;
                std::copy(data, data + common::min(packed_size, sizeof(T)), reinterpret_cast<std::uint8_t *>(&object));

                return std::make_optional<T>(std::move(object));
            }

            /**
             * \brief Set the status of this request.
             * 
             * When you set the status of a request, the thread that requests this message will be signaled.
             * This function handles the situation so that the target thread will only be signaled once.
             * 
             * \param res The result code.
             */
            void complete(int res);

            /**
             * \brief    Get the flag contains IPC argument information.
             * \returns  The flag value.
             * 
             * \sa       complete
             */
            int flag() const;

            /**
             * \brief   Get a pointer to the data of an IPC descriptor argument.
             * 
             * \param   idx The index of the argument. Should be in the range [0, 3].
             * \returns Null if the index is out of range, or the IPC argument is not a descriptor.
             *          Else, return pointer to the descriptor data.
             * 
             * \sa      get_argument_data_size, set_descriptor_argument_length
            */
            std::uint8_t *get_descriptor_argument_ptr(int idx);

            /**
             * \brief   Get the size of data stored in the IPC argument.
             * 
             * - If the data is an unspecified or integer, the size will be 4.
             * - If the data is an descriptor, the size will be the raw size of character data.
             * 
             * \param   idx The index of argument.
             * \returns Size of the IPC argument in the specified index.
             *          Return size_t(-1) if index is out of range.
             * 
             * \sa      get_descriptor_argument_ptr
             */
            std::size_t get_argument_data_size(int idx);

            /**
             * \brief   Get the max size of data stored in the IPC argument.
             * 
             * - If the data is an unspecified or integer, the size will be 4.
             * - If the data is an descriptor, the size will be the max size of character data.
             * 
             * \param   idx The index of argument.
             * \returns Size of the IPC argument in the specified index.
             *          Return size_t(-1) if index is out of range.
             * 
             * \sa      get_descriptor_argument_ptr
             */
            std::size_t get_argument_max_data_size(int idx);

            /**
             * \brief   Set length of a descriptor passed as IPC argument in given index.
             * 
             * \param   idx The index of the argument. Must be in range of [0, 3].
             * \param   len The new length to set to target descriptor.
             * 
             * \returns True if success. False if failure (index out of range, length to large,
             *          or IPC argument is not a descriptor).
             * 
             * \sa      get_argument_data_size, get_argument_max_data_size
             */
            bool set_descriptor_argument_length(const int idx, const std::uint32_t len);

            /**
             * \brief    Write integer data to an IPC argument.
             * 
             * The IPC argument should have type of handle.
             * 
             * \param    idx  The index of the IPC argument, in range of [0, 3].
             * \param    data The integer data
             * 
             * \returns  True on success.
             */
            bool write_arg(int idx, uint32_t data);

            /**
             * \brief    Write 16-bit string to an IPC argument.
             * 
             * The IPC argument should have type of 16-bit descriptor.
             * 
             * \param    idx  The index of the IPC argument, in range of [0, 3].
             * \param    data The 16-bit string.
             * 
             * \returns  True on success.
             * \sa       write_data_to_descriptor_argument
             */
            bool write_arg(int idx, const std::u16string &data);

            /**
             * \brief    Write raw binary data to an IPC argument.
             * 
             * The IPC argument should have type of 8-bit descriptor.
             * 
             * Optional error code lookup:
             * - (0)  for success.
             * - (-1) if the IPC argument type is not 8-bit descriptor.
             * - (-2) if the descriptor is not large enough to hold the data.
             * 
             * \param    idx                 The index of the IPC argument, in range of [0, 3].
             * \param    data                Pointer to start of data.
             * \param    len                 The length of binary data.
             * \param    err_code            Optional pointer contains error code if fail.
             * \param    auto_shrink_to_fit  If this is true, the binary data length will be shrinked to fit
             *                               the descriptor.
             * 
             * \returns  True on success.
             * \sa       write_arg
             */
            bool write_data_to_descriptor_argument(int idx, const uint8_t *data, uint32_t len, int *err_code = nullptr, const bool auto_shrink_to_fit = false);

            /**
             * \brief    Write a struct to an IPC argument.
             * 
             * The IPC argument should have type of 8-bit descriptor.
             * 
             * For optional error code lookup, see write_data_to_descriptor_argument uint8_t* version
             * 
             * \param    idx                 The index of the IPC argument, in range of [0, 3].
             * \param    data                A reference to the struct we want to write.
             * \param    err_code            Optional pointer contains error code if fail.
             * \param    auto_shrink_to_fit  If this is true, the struct will be shrinked to fit
             *                               the descriptor.
             * 
             * \returns  True on success.
             * \sa       write_arg
             */
            template <typename T>
            bool write_data_to_descriptor_argument(int idx, const T &data, int *err_code = nullptr, const bool auto_shrink_to_fit = false) {
                return write_data_to_descriptor_argument(idx, reinterpret_cast<const uint8_t *>(&data), sizeof(T), err_code, auto_shrink_to_fit);
            }

            /**
             * \brief   Check if the process that owns the thread asked for this request, satisfy the security
             *          policy defined.
             * 
             * \param    policy   Defined policy that the target process must be satisfied.
             * \param    missing  Points to the security info struct, that will be filled with missing security info
             *                    that the process required to pass the given policy. Null for not needed.
             * 
             * \returns  True if passed.
             */
            bool satisfy(epoc::security_policy &policy, epoc::security_info *missing = nullptr);
        };
    }
}
