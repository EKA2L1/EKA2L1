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

#include <common/vecx.h>
#include <epoc/services/window/classes/winbase.h>

#include <tuple>
#include <vector>

namespace eka2l1 {
    class window_server;
}

namespace eka2l1::epoc {
    /**
     * \brief Walk through all windows and deliver pointer to those who are applicable.
     */
    struct window_pointer_focus_walker : public epoc::window_tree_walker {
        std::vector<std::pair<epoc::event, bool>> evts_;
        eka2l1::vec2 scr_coord_;

        void add_new_event(const epoc::event &evt);
        void process_event_to_target_window(epoc::window *win, epoc::event &evt);

        bool do_it(epoc::window *win) override;

        void clear();
    };

    /**
     * \brief Deliver key events to windows that are suitable.
     */
    struct window_key_shipper {
        eka2l1::window_server *serv_; ///< Pointer to window server.

        std::vector<epoc::event> evts_;

        explicit window_key_shipper(window_server *serv);

        /**
         * \brief Queue new event to be delivered. This must be a key event.
         * 
         * \param evt   Reference to the event.
         */
        void add_new_event(const epoc::event &evt);

        /**
         * \brief Start delivering key events.
         */
        void start_shipping();
    };
}