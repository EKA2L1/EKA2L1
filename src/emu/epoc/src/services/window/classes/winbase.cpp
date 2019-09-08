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

#include <epoc/services/window/classes/winbase.h>
#include <epoc/services/window/op.h>
#include <epoc/services/window/opheader.h>
#include <epoc/services/window/window.h>
#include <epoc/services/window/screen.h>

#include <epoc/timing.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1::epoc {
    void window::queue_event(const epoc::event &evt) {
        client->queue_event(evt);
    }

    void window::set_parent(window *new_parent) {
        // New one will be the oldest. Quirky, but how WSERV functions.
        new_parent = parent;
        sibling = parent->child;
        parent->child = this;
    }

    window *window::root_window() {
        return scr->root;
    }
    
    void window::walk_tree(window_tree_walker *walker, const window_tree_walk_style style) {
        window *end = root_window();
        window *cur = this;
        window *sibling = cur->sibling;
        window *parent = cur->parent;

        if (style != window_tree_walk_style::bonjour_previous_siblings) {
            if (style == window_tree_walk_style::bonjour_children) {
                end = this;
            }

            sibling = cur;

            while (sibling->child != nullptr) {
                sibling = sibling->child;
            }
        }

        do {
            if (sibling != nullptr) {
                // Traverse to our next sibling
                cur = sibling;

                // If this has a child, traverse to the oldest possible child.
                // We can start walking again from bottom. Don't question much, this code is like why does it work idk.
                // TODO (pent0): figure out the magic behind this. This is ripped from OSS code.
                while (cur->child != nullptr) {
                    cur = cur->child;
                }
            } else {
                // No more sibling present. Get to the parent.
                cur = parent;
            }

            parent = cur->parent;
            sibling = cur->sibling;

            if (cur->type == window_kind::group && walker->do_it(cur)) {
                return;
            }
        } while (cur != end);
    }
    
    void window::set_position(const int new_pos) {
        if (check_order_change(new_pos)) {
            move_window(parent, new_pos);
        }
    }

    void window::move_window(epoc::window *new_parent, const int new_pos) {
        if (type == window_kind::group || type == window_kind::client || new_parent != parent) {
            // TODO: Check if any childs need a redraw before hassle.
            client->get_ws().get_anim_scheduler()->schedule(client->get_ws().get_graphics_driver(),
                scr, client->get_ws().get_timing_system()->get_global_time_us());
        }

        remove_from_sibling_list();

        // The window that will be previous sibling of our future window.
        window **prev = &new_parent->child;

        // Iterates until finding a younger window
        while (*prev != nullptr && priority < (*prev)->priority) {
            prev = &((*prev)->sibling);
        }

        int pos = new_pos;

        // Iterates to find right window position.
        while (pos-- != 0 && *prev != nullptr && (*prev)->priority == priority) {
            prev = &((*prev)->sibling);
        }

        // Link to the list
        sibling = *prev;
        parent = new_parent;
        *prev = this;
    }

    bool window::check_order_change(const int new_pos) {
        // The more soon we reach the window in the linked list, the higher priority it's.
        window *cur = parent->child;
        window *prev = nullptr;

        // If the current window is the head: iterates to next child right away.
        // Iterates until we meet a window that has smaller or equal priority then us.
        while (cur == this || (cur != nullptr && priority < cur->priority)) {
            prev = cur;
            cur = cur->sibling;
        }

        if (prev == this) {
            cur = this;
        } else if (cur == nullptr || (cur->sibling == this && priority > cur->sibling->priority)) {
            // If there is no window that has smaller priority then us, we need to order this at the end
            // of the list. That means there is some order change
            //
            // Case 2: this window is the next sibling of the current. if the priority of the current window is larger
            // then it's in the wrong order and really can't do a position check (since priority not equal).
            return true;
        }

        // Traverse and find our current window, and see if we need to change order
        int pos = new_pos;
        
        while (pos-- != 0 && cur->sibling != nullptr && priority == cur->sibling->priority) {
            cur = cur->sibling;
        }

        return (cur != this);
    }
    
    void window::remove_from_sibling_list() {
        window *ite = parent->child;

        if (parent->child == this) {
            // Make the next sibling oldest
            parent->child = sibling;
            return;
        }

        while (true) {
            if (ite->sibling == this) {
                break;
            }

            ite = ite->sibling;
        }

        ite->sibling = sibling;
    }

    // This is the biggest sin i have commit, using DFS
    // 9:10 PM 5/9/2019 pent0
    void walk_tree_back_to_front(window *start, window_tree_walker *walker) {
        if (start == nullptr) {
            return;
        }

        if (start->type != window_kind::group && walker->do_it(start)) {
            return;
        }

        // Gone through all siblings, walk on their childs
        // TODO: Is this correct? Older guy, children of older guy, then newer guy, children of newer guy.
        while (start != nullptr) {
            walk_tree_back_to_front(start->child, walker);
            start = start->sibling;
        }
    }

    void window::walk_tree_back_to_front(window_tree_walker *walker) {
        eka2l1::epoc::walk_tree_back_to_front(this, walker);
    }

    int window::ordinal_position(const bool full) {
        window *win = parent->child;

        if (full) {
            while (win->priority > priority) {
                win = win->sibling;
            }
        }

        int count = 0;

        for (count = 0; win != this; count++) {
            win = win->sibling;
        }

        return count;
    }

    bool window::execute_command_for_general_node(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd cmd) {
        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpEnableModifierChangedEvents: {
            epoc::event_mod_notifier_user nof;
            nof.notifier = *reinterpret_cast<event_mod_notifier *>(cmd.data_ptr);
            nof.user = this;

            client->add_event_notifier<epoc::event_mod_notifier_user>(nof);
            ctx.set_request_status(KErrNone);

            return true;
        }

        case EWsWinOpSetOrdinalPosition: {
            const int position = *reinterpret_cast<int *>(cmd.data_ptr);
            set_position(position);

            ctx.set_request_status(KErrNone);

            return true;
        }

        case EWsWinOpSetOrdinalPositionPri: {
            ws_cmd_ordinal_pos_pri *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
            priority = info->pri1;
            const int position = info->pri2;

            set_position(position);
            ctx.set_request_status(KErrNone);

            return true;
        }

        case EWsWinOpIdentifier: {
            ctx.set_request_status(static_cast<int>(id));
            return true;
        }

        case EWsWinOpEnableErrorMessages: {
            epoc::event_control ctrl = *reinterpret_cast<epoc::event_control *>(cmd.data_ptr);
            epoc::event_error_msg_user nof;
            nof.when = ctrl;
            nof.user = this;

            client->add_event_notifier<epoc::event_error_msg_user>(nof);
            ctx.set_request_status(KErrNone);

            return true;
        }

        default: {
            break;
        }
        }

        return false;
    }
}
