/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <utils/reqsts.h>

#include <cstddef>
#include <cstdint>
#include <functional>

namespace eka2l1::epoc::socket {
    struct saddress;

    enum {
        SOCKET_MESSAGE_SIZE_IS_STREAM = 0, ///< Stream socket, message can be of any size
        SOCKET_MESSAGE_SIZE_UNDEFINED = 1, ///< Undefined, depends on layers
        SOCKET_MESSAGE_SIZE_NO_LIMIT = -1
    };
    
    enum socket_operate_flag : std::uint32_t {
        SOCKET_FLAG_DONT_WAIT_FULL = 1 << 29
    };

    enum socket_ioctl_level {
        SOCKET_IOCTL_LEVEL_BASE = 1,
        SOCKET_IOCTL_LEVEL_SMS_PROV = 0x100
    };

    enum socket_ioctl_command_base {
        SOCKET_IOCTL_LEVEL_BASE_COMMAND_SELECT = 1
    };

    using receive_done_callback = std::function<void(const std::int64_t received)>;

    struct socket {
    public:
        virtual ~socket() = default;

        /**
         * @brief Get an option available from current socket.
         * 
         * @param option_id         The ID of the option.
         * @param option_family     The ID of the family that this option belongs to.
         * @param buffer            Destination to write data to.
         * @param avail_size        The total size that the destination buffer can hold.
         * 
         * @returns size_t(-1) if there is an error or not enough sufficient size to hold, else returns the total size written.
         */
        virtual std::size_t get_option(const std::uint32_t option_id, const std::uint32_t option_family,
            std::uint8_t *buffer, const std::size_t avail_size);

        /**
         * @brief Set an option available from current socket.
         * 
         * @param option_id         The ID of the option.
         * @param option_family     The ID of the family that this option belongs to.
         * @param buffer            Data source to set.
         * @param avail_size        The total size that the source buffer holds.
         * 
         * @returns false on failure.
         */
        virtual bool set_option(const std::uint32_t option_id, const std::uint32_t option_family,
            std::uint8_t *buffer, const std::size_t avail_size);

        /**
         * @brief Perform asynchronous I/O control operation.
         * 
         * @param command           The command number to execute.
         * @param complete_info     Notify info signals the completion of this control operation.
         * @param buffer            Pointer to a buffer which data can be sent/received through.
         * @param available_size    The input size that the upper buffer can hold.
         * @param max_buffer_size   The maximum size that the upper buffer can hold.
         * @param level             Control operation level.
         */
        virtual void ioctl(const std::uint32_t command, epoc::notify_info &complete_info, std::uint8_t *buffer,
            const std::size_t available_size, const std::size_t max_buffer_size, const std::uint32_t level);

        /**
         * @brief Perform binding this address to local desired address.
         * 
         * @param sockaddr_buffer        Buffer containing the information about the address, different per socket implementation.
         * @param info                   Request status notify info.
         */
        virtual void bind(const saddress &addr, epoc::notify_info &info);

        /**
         * @brief Perform connect to remote address.
         * 
         * @param addr                  Address to be connected to
         * @param info                  Request status notify info.
         */
        virtual void connect(const saddress &addr, epoc::notify_info &info);

        /**
         * @brief Send data to remote host, on a non-connected or connected socket.
         * 
         * If a socket is not connected, sockaddr must not be NULL and points to a valid buffer containg
         * information about remote host address.
         * 
         * @param   data                    Data to send to remote host.
         * @param   data_size               Size of data to send, also the size of data buffer.
         * @param   sent_size               Optional arugment that contains the amount of data sent. Can be NULL.
         * @param   sockaddr                The address to send the data to. On a connected socket, this address will be used if not NULL.
         * @param   flags                   Flags that specified how to process this packet.
         * @param   complete_info           A notify info that is signaled when the sent is done, can contain the error code.
         */
        virtual void send(const std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *sent_size, const saddress *addr,
            const std::uint32_t flags, epoc::notify_info &complete_info);

        /**
         * @brief Receive data from remote client, on a non-connected or connected socket.
         * 
         * If a socket is not connected, sockaddr must not be NULL and points to a valid buffer containg
         * information about remote address.
         * 
         * @param   data                    Data to be received from remote.
         * @param   data_size               Size of data to be read. For stream socket, to receive the data pack available only, use the flag SOCKET_FLAG_DONT_WAIT_FULL.
         * @param   sent_size               Optional arugment that on completion contains the amount of data read. Can be NULL.
         * @param   sockaddr                The address to receive the data from. On a connected socket, this address will only be used if not NULL and the socket is stream.
         * @param   flags                   Flags that specified how to process this packet.
         * @param   complete_info           A notify info that is signaled when the receive is done, can contain the error code.
         * @param   done_callback           Function that will be called when receive is done (asynchronously).
         */
        virtual void receive(std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *recv_size, const saddress *addr,
            const std::uint32_t flags, epoc::notify_info &complete_info, receive_done_callback done_callback = nullptr);

        virtual void cancel_receive();

        virtual void cancel_send();

        virtual void cancel_connect();
    };
}