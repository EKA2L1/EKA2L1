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

#include <services/window/classes/wsobj.h>
#include <services/window/common.h>

#include <common/linked.h>
#include <common/vecx.h>

#include <memory>

namespace eka2l1::epoc {
    struct window;

    enum class window_kind {
        normal,
        group,
        top_client,
        client
    };

    enum class window_tree_walk_style {
        bonjour_previous_siblings,
        bonjour_children,
        bonjour_children_and_previous_siblings
    };

    /**
     * \brief Class to hook for tree walking.
     */
    struct window_tree_walker {
        /**
         * \brief Handle function for a window.
         */
        virtual bool do_it(window *win) = 0;
    };

    void walk_tree_back_to_front(window *start, window_tree_walker *walker);

    /** \brief Base class for all window. */
    struct window : public window_client_obj {
        window *parent{ nullptr }; ///< Pointer to the parent
        window *sibling{ nullptr }; ///< Pointer to oldest sibling
        window *child{ nullptr }; ///< Pointer to the oldest child

        // The priority of the window.
        std::int32_t priority{ 0 };
        std::uint32_t client_handle{ 0 };
        
        ws::uid focus_group_change_event_handle{ 0 };

        window_kind type;

        enum {
            flags_shadow_disable = 1 << 0,
            flags_active = 1 << 1,
            flags_visible = 1 << 2,
            flags_allow_pointer_grab = 1 << 3,
            flags_non_fading = 1 << 4,
            flags_enable_alpha = 1 << 5,
            flags_faded = 1 << 6,
            flags_faded_default_param = 1 << 7,
            flags_faded_also_children = 1 << 8,
            flags_dsa = 1 << 9,
            flags_enable_pbe = 1 << 10,
            flags_in_redraw = 1 << 11,
            flag_focus_receiveable = 1 << 12,
            flag_winmode_fixed = 1 << 13,
            flag_visiblity_event_report = 1 << 14,
            flag_content_changed = 1 << 16,
            flag_shape_region = 1 << 17,            // Only support region and square on the emulator, others are too complicated
            flag_fix_native_orientation = 1 << 18   // Surface created by EGL will retain width and height of the screen when the phone is in its normal orientation
        };

        std::uint32_t flags;

        std::uint8_t black_map = 128;
        std::uint8_t white_map = 255;

        std::uint32_t get_client_handle() const {
            return client_handle;
        }

        void set_client_handle(const std::uint32_t new_handle) {
            client_handle = new_handle;
        }

        /***
         * \brief Get the root window.
         */
        window *root_window();

        /**
         * \brief   Get the ordinal position of the window.
         * 
         * \param   full Enable traversing to siblings with same priority, then find the position there.
         * 
         * \returns The ordinal position of this window.
         */
        int ordinal_position(const bool full);

        /**
         * \brief Handler for InquireOffset opcode.
         * 
         * The offset calculates origin distance between this window and a targeted window given by the handle.
         */
        void inquire_offset(service::ipc_context &ctx, ws_cmd &cmd);

        void set_fade(service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void window_group_id(service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        bool execute_command_for_general_node(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);

        /*! \brief Generic event queueing
        */
        virtual void queue_event(const epoc::event &evt);

        /**
         * \brief Get the origin of window.
         */
        virtual eka2l1::vec2 get_origin();

        /**
         * \brief Walk through window tree, with the root from the current window.
         * 
         * The function walks through the tree in order from front to back:
         * - Child windows will be in front of parent windows.
         * 
         * \param walker The class hooking the walk.
         * \param style  Walk style.
         */
        void walk_tree(window_tree_walker *walker, const window_tree_walk_style style);

        void walk_tree_back_to_front(window_tree_walker *walker);

        void move_window(epoc::window *new_parent, const int new_pos);

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
            , type(window_kind::normal)
            , parent(nullptr)
            , sibling(nullptr)
            , child(nullptr)
            , flags(flags_visible) {
            set_parent(parent);
        }

        explicit window(window_server_client_ptr client, screen *scr, window *parent, window_kind type)
            : window_client_obj(client, scr)
            , type(type)
            , parent(nullptr)
            , sibling(nullptr)
            , child(nullptr)
            , flags(flags_visible) {
            set_parent(parent);
        }

        void set_initial_state() {
            set_position(0);

            if (parent && ((parent->flags & flags_visible) == 0)) {
                flags &= ~flags_visible;
            }
        }

        ~window() override;
    };
}
