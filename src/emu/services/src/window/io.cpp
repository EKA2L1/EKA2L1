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

#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/io.h>
#include <services/window/window.h>

#include <kernel/kernel.h>

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

        kernel_system *kern = win->client->get_ws().get_kernel_system();

        kern->lock();
        win->queue_event(evt);
        kern->unlock();
    }

    bool window_pointer_focus_walker::do_it(epoc::window *win) {
        if (evts_.empty()) {
            return true;
        }

        if (win->type != epoc::window_kind::client) {
            return false;
        }

        epoc::window_user *user = reinterpret_cast<epoc::window_user *>(win);

        std::optional<eka2l1::rect> contain_area;
        auto contain_rect_this_mode_ite = user->scr->pointer_areas_.find(user->scr->crr_mode);

        if (contain_rect_this_mode_ite != user->scr->pointer_areas_.end()) {
            contain_area = contain_rect_this_mode_ite->second;
        }

        for (auto &[evt, sent_to_highest_z] : evts_) {
            // Is this event really in the pointer area, if area exists. If not, pass
            if (contain_area && !contain_area->contains(evt.adv_pointer_evt_.pos)) {
                continue;
            }

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

            if (!sent_to_highest_z) {
                if (window_rect.contains(evt.adv_pointer_evt_.pos)) {
                    scr_coord_ = evt.adv_pointer_evt_.pos;

                    process_event_to_target_window(win, evt);
                    sent_to_highest_z = true;
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

    static const bool is_device_std_key_not_repeatable(const std_scan_code code) {
        return (code >= std_key_device_0) && (code <= std_key_device_1);
    }

    void window_key_shipper::start_shipping() {
        if (evts_.empty()) {
            return;
        }

        epoc::window_group *focus = serv_->get_focus();

        if (!focus) {
            return;
        }

        int ui_rotation = focus->scr->ui_rotation;

        for (auto &evt : evts_) {
            evt.key_evt_.scancode = epoc::post_processing_scancode(static_cast<epoc::std_scan_code>(evt.key_evt_.scancode),
                ui_rotation);

            bool dont_send_extra_key_event = (evt.type != epoc::event_code::key_down);

            // TODO: My assumption... For now.
            // Actually this smells like a hack
            const bool repeatable = !is_device_std_key_not_repeatable(static_cast
                <epoc::std_scan_code>(evt.key_evt_.scancode));

            epoc::event extra_event = evt;
            extra_event.type = epoc::event_code::key;

            kernel_system *kern = focus->client->get_ws().get_kernel_system();
            ntimer *timing = kern->get_ntimer();

            const std::uint32_t the_code = epoc::map_scancode_to_keycode(static_cast<std_scan_code>(
                evt.key_evt_.scancode));

            const std::uint64_t data_for_repeatable = extra_event.key_evt_.scancode | (static_cast<std::uint64_t>(
                extra_event.key_evt_.code) << 32);

            if (!dont_send_extra_key_event) {
                extra_event.key_evt_.code = the_code;
                extra_event.time = kern->home_time();

                if (repeatable)
                    extra_event.key_evt_.modifiers = event_modifier_repeatable;
            }

            evt.handle = focus->get_client_handle();
            extra_event.handle = focus->get_client_handle();

            kern->lock();
            focus->queue_event(evt);
            kern->unlock();

            if (!dont_send_extra_key_event) {
                // Give it a single key event also
                kern->lock();
                focus->queue_event(extra_event);
                kern->unlock();

                if ((evt.type == epoc::event_code::key_down) && repeatable) {
                    timing->schedule_event(serv_->initial_repeat_delay_, serv_->repeatable_event_, data_for_repeatable);
                }
            }

            if ((evt.type == epoc::event_code::key_up) && repeatable)
                timing->unschedule_event(serv_->repeatable_event_, data_for_repeatable);

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

                    kern->lock();
                    ite->user->queue_event(extra_event);
                    kern->unlock();

                    break;

                case epoc::event_key_capture_type::up_and_downs:
                    evt.handle = ite->user->get_client_handle();

                    kern->lock();
                    ite->user->queue_event(evt);
                    kern->unlock();

                    break;

                default:
                    break;
                }
            }
        }

        evts_.clear();
    }
}