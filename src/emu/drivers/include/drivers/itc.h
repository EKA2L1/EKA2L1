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

            T ret = *(reinterpret_cast<T*>(&data[0]));
            data.erase(0, sizeof(T));

            return ret;
        }

        /*! \brief Push result or parameter of type T to the context.
            \params val The target value.
        */
        template <typename T>
        void push(const T val) {
            data.append(reinterpret_cast<const char*>(&val), sizeof(val));
        }

        void push_string(const std::string val) {
            std::uint32_t len = static_cast<std::uint32_t>(val.length());
            push(len);
            data.append(val.begin(), val.end());
        }

        std::optional<std::string> pop_string() {
            auto len = pop<std::uint32_t>();

            if (!len || *len > data.size()) {
                return std::nullopt;
            }

            std::string ret = data.substr(0, *len);
            data.erase(0, *len);

            return ret;
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

        bool already_locked = false;

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
        
        void lock_driver_from_process();
        void unlock_driver_from_process();
    };

    class graphics_driver_client: public driver_client {
    public:
        graphics_driver_client() {}

        explicit graphics_driver_client(driver_instance driver);

        /*! \brief Get the screen size in pixels
        */
        vec2 screen_size();

        /*! \brief Set screen size in pixels
        */
        void set_screen_size(eka2l1::vec2 &s);

        /*! \brief Starting a scissor test, allow redraw only in specified area.
         *
         *  Use in drawing window rect or invalidate a specific region of an window
        */
        void invalidate(eka2l1::rect &rect);

        void end_invalidate();

        /*! \brief Clear the screen with color.
            \params color A RGBA vector 4 color
        */
        void clear(vecx<int, 4> color);

        /*! \brief Draw text with bound rect
        */
        void draw_text(eka2l1::rect rect, const std::string &str);
    };
}

namespace eka2l1 {
    using graphics_driver_client_ptr = std::shared_ptr<drivers::graphics_driver_client>;
}