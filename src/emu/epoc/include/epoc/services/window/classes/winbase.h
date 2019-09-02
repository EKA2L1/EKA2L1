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

#include <common/linked.h>
#include <memory>

namespace eka2l1::epoc {
    struct window;
    
    enum class window_kind {
        normal,
        group,
        top_client,
        client
    };

    /** \brief Base class for all window. */
    struct window : public window_client_obj {
        window *parent{ nullptr };          ///< Pointer to the parent
        window *sibling{ nullptr };         ///< Pointer to oldest sibling
        window *child{ nullptr  };          ///< Pointer to the oldest child

        // The priority of the window.
        std::uint16_t priority{ 0 };

        window_kind type;

        bool execute_command_for_general_node(eka2l1::service::ipc_context &ctx,
            eka2l1::ws_cmd cmd);

        /*! \brief Generic event queueing
        */
        virtual void queue_event(const epoc::event &evt);

        /**
         * \brief Set position of the window.
         * 
         * \param new_pos      New position of the window.
         */
        void set_position(const int new_pos);

        /**
         * \brief Check if the current priority and the given window position will requires
         *        window order change.
         * 
         * With sibling windows, a window is consider "older" when it has greater priority then
         * other siblings. If multiple windows have the same priority, the one that go first will
         * be drawn.
         * 
         * This function first check:
         * - Does the priority of the window feels weird? Should it be reorder?
         * - Then... Does the position of the window feels weird? Like it should be at the front
         *   of other window with same priority.
         * 
         * \param new_pos The new position of the window, compares to other siblings with same priority.
         * 
         * \returns True if the window needs to be reordered.
         */
        bool check_order_change(const int new_pos);
        
        /**
         * \brief Remove the window from the sibling list.
         */
        void remove_from_sibling_list();

        void set_parent(window *parent);

        explicit window(window_server_client_ptr client, screen *scr, window *parent)
            : window_client_obj(client, scr)
            , type(window_kind::normal) {
            set_parent(parent);
        }

        explicit window(window_server_client_ptr client, screen *scr, window *parent, window_kind type)
            : window_client_obj(client, scr)
            , type(type) {
            set_parent(parent);
        }
    };
}
