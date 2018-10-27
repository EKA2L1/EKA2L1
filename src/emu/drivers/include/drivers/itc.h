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

#include <common/vecx.h>

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <optional>

namespace eka2l1::drivers {
    /*! \brief A context to communicate between client and EKA2L1's driver
               which may be on different thread.
    */
    struct itc_context {
        std::string data;

        itc_context() {}

        /*! \brief   Pop the value with type T from the context.
            \returns The value needed.
        */
        template <typename T>
        std::optional<T> pop() {
            if (data.size() < sizeof(T)) {
                return std::optional<T>{};
            }

            T ret = *(reinterpret_cast<T*>(data[data.size() - 1 - sizeof(T)]));
            data.erase(data.begin() + data.size() - 1 - sizeof(T), data.end());

            return ret;
        }

        /*! \brief Push result or parameter of type T to the context.
            \params val The target value.
        */
        template <typename T>
        void push(const T val) {
            data.append(reinterpret_cast<const char*>(&val), sizeof(val));
        }
    };

    class driver;
    using driver_instance = std::shared_ptr<driver>;

    /*! \brief Base class for all driver client, which requested a ITC function.
    */
    class driver_client {
    protected:
        driver_instance driver;
        std::mutex lock;

        /*! \brief Send the opcode to the driver, provided also with the context.
            \returns True if the send is successfully. Results will be pushed to the context.
        */
        bool send_opcode(const int opcode, itc_context &ctx);

    public:
        driver_client() = default;

        explicit driver_client(driver_instance driver);
        virtual ~driver_client() {}

        /*! \brief Wait for driver. The client locked, waiting for the driver. When driver
                   finished its job (work queue done), notfy all threads
        */
        void sync_with_driver();
    };

    class graphics_driver_client: public driver_client {
    public:
        graphics_driver_client() {}

        explicit graphics_driver_client(driver_instance driver);

        /*! \brief Get the screen size in pixels
        */
        vec2 screen_size();

        /*! \brief Clear the screen with color.
            \params color A RGBA vector 4 color
        */
        void clear(vecx<int, 4> color);
    };
}

namespace eka2l1 {
    using graphics_driver_client_ptr = std::shared_ptr<drivers::graphics_driver_client>;
}