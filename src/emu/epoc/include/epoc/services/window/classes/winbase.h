/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/window/classes/wsobj.h>
#include <epoc/services/window/common.h>

#include <common/queue.h>

#include <memory>

namespace eka2l1::epoc {
    struct window;
    using window_ptr = std::shared_ptr<epoc::window>;

    enum class window_kind {
        normal,
        group,
        top_client,
        client
    };

    struct screen_device;
    using screen_device_ptr = std::shared_ptr<epoc::screen_device>;

    using window_client_obj_ptr = std::shared_ptr<window_client_obj>;

    /** \brief Base class for all window. */
    struct window : public window_client_obj {
        eka2l1::cp_queue<window_ptr> childs;
        screen_device_ptr dvc;

        window_ptr parent;

        // It's just z value. The second one will be used when there is
        // multiple window with same first z.
        std::uint16_t priority{ 0 };
        std::uint16_t secondary_priority{ 0 };

        std::uint16_t redraw_priority();
        virtual void priority_updated();

        window_kind type;

        bool operator==(const window &rhs) {
            return priority == rhs.priority;
        }

        bool operator!=(const window &rhs) {
            return priority != rhs.priority;
        }

        bool operator>(const window &rhs) {
            return priority > rhs.priority;
        }

        bool operator<(const window &rhs) {
            return priority < rhs.priority;
        }

        bool operator>=(const window &rhs) {
            return priority >= rhs.priority;
        }

        bool operator<=(const window &rhs) {
            return priority <= rhs.priority;
        }

        bool execute_command_for_general_node(eka2l1::service::ipc_context &ctx,
            eka2l1::ws_cmd cmd);

        /*! \brief Generic event queueing
        */
        virtual void queue_event(const epoc::event &evt);

        window(window_server_client_ptr client)
            : window_client_obj(client)
            , type(window_kind::normal)
            , dvc(nullptr) {}

        window(window_server_client_ptr client, window_kind type)
            : window_client_obj(client)
            , type(type)
            , dvc(nullptr) {}

        window(window_server_client_ptr client, screen_device_ptr dvc, window_kind type)
            : window_client_obj(client)
            , type(type)
            , dvc(dvc) {}
    };
}
