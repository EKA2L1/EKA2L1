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

#include <epoc/services/window/io.h>
#include <epoc/services/window/window.h>
#include <epoc/services/window/classes/wingroup.h>
#include <epoc/services/window/classes/winuser.h>

namespace eka2l1::epoc {
    void window_pointer_focus_walker::add_new_event(const epoc::event &evt) {
        evts_.emplace_back(evt, false);
    }

    void window_pointer_focus_walker::process_event_to_target_window(epoc::window *win, epoc::event &evt) {
        assert(win->type == epoc::window_kind::client);

        epoc::window_user *user = reinterpret_cast<epoc::window_user *>(win);
        // Stop, we found it!
        // Send it right now
        evt.adv_pointer_evt_.pos = scr_coord_ - user->pos;

        if (user->parent->type == epoc::window_kind::top_client) {
            evt.adv_pointer_evt_.parent_pos = scr_coord_;
        } else {
            // It must be client kind
            assert(user->parent->type == epoc::window_kind::client);
            evt.adv_pointer_evt_.parent_pos = reinterpret_cast<epoc::window_user *>(user->parent)->pos
                + evt.adv_pointer_evt_.pos;
        }

        evt.handle = win->get_client_handle();
        win->queue_event(evt);
    }

    bool window_pointer_focus_walker::do_it(epoc::window *win) {
        if (evts_.empty()) {
            return true;
        }

        if (win->type != epoc::window_kind::client) {
            return false;
        }

        epoc::window_user *user = reinterpret_cast<epoc::window_user *>(win);

        for (auto &[evt, sended_to_highest_z]: evts_) {
            const bool filter_enter_exit = ((evt.type == epoc::event_code::touch_enter) || (evt.type == epoc::event_code::touch_exit))
                && (user->filter & epoc::pointer_filter_type::pointer_enter);

            const bool filter_drag = evt.adv_pointer_evt_.evtype == epoc::event_type::drag && (user->filter & epoc::pointer_filter_type::pointer_drag);

            // Filter out events, assuming move event never exist (phone)
            // When you use touch on your phone, you drag your finger. Move your mouse simply doesn't exist.
            if (filter_enter_exit || filter_drag) {
                continue;
            }

            eka2l1::rect window_rect{ user->pos, user->size };
            evt.type = epoc::event_code::touch;

            if (!sended_to_highest_z) {
                if (window_rect.contains(evt.adv_pointer_evt_.pos)) {
                    scr_coord_ = evt.adv_pointer_evt_.pos;

                    process_event_to_target_window(win, evt);
                    sended_to_highest_z = true;
                }

                continue;
            } else {
                // Check to see if capture flag is enabled
                // TODO:
            }
        }

        return false;
    }
    
    void window_pointer_focus_walker::clear() {
        evts_.clear();
    }

    window_key_shipper::window_key_shipper(window_server *serv)
        : serv_(serv) {

    }

    void window_key_shipper::add_new_event(const epoc::event &evt) {
        evts_.push_back(evt);
    }

    void window_key_shipper::start_shipping() {
        if (evts_.empty()) {
            return;
        }

        epoc::window_group *focus = serv_->get_focus();
        int ui_rotation = focus->scr->ui_rotation;

        for (auto &evt: evts_) {
            evt.key_evt_.scancode = epoc::map_inputcode_to_scancode(evt.key_evt_.scancode,
                ui_rotation);

            bool dont_send_extra_key_event = false;

            if (!serv_->key_block_active) {
                // Don't block simultaneous key presses.
                // Also looks like the ::key event are ignored. TODO(pent0): Only these buttons?
                dont_send_extra_key_event = true;
            } else {
                dont_send_extra_key_event = (evt.type != epoc::event_code::key_down);
            }

            epoc::event extra_event = evt;
            extra_event.type = epoc::event_code::key;

            if (!dont_send_extra_key_event) {
                extra_event.key_evt_.code =  epoc::map_scancode_to_keycode(static_cast<TStdScanCode>(
                    evt.key_evt_.scancode));
            }

            evt.handle = focus->get_client_handle();
            extra_event.handle = focus->get_client_handle();

            focus->queue_event(evt);

            if (!dont_send_extra_key_event) {
                // Give it a single key event also
                focus->queue_event(extra_event);
            }

            // Iterates through key capture requests and deliver those in needs.key_capture_request_queue &rqueue = key_capture_requests[extra_key_evt.key_evt_.code];
            window_server::key_capture_request_queue &rqueue = serv_->key_capture_requests[evt.key_evt_.code];

            for (auto ite = rqueue.end(); ite != rqueue.begin(); ite--) {
                // No need to deliver twice.
                if (ite->user->id == focus->id) {
                    break;
                }

                switch (ite->type_) {
                case epoc::event_key_capture_type::normal:
                    extra_event.handle = ite->user->get_client_handle();
                    ite->user->queue_event(extra_event);
                    break;

                case epoc::event_key_capture_type::up_and_downs:
                    evt.handle = ite->user->get_client_handle();
                    ite->user->queue_event(evt);

                    break;

                default:
                    break;
                }
            }
        }

        evts_.clear();
    }
}