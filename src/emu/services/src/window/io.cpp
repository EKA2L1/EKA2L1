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

    static void prepare_event_for_window(epoc::canvas_base *user, const eka2l1::vec2 &scr_coord, epoc::event &evt) {
        evt.adv_pointer_evt_.pos = scr_coord - user->absolute_position();

        if (user->parent->type == epoc::window_kind::top_client) {
            evt.adv_pointer_evt_.parent_pos = scr_coord;
        } else {
            // It must be client kind
            assert(user->parent->type == epoc::window_kind::client);
            evt.adv_pointer_evt_.parent_pos = scr_coord - reinterpret_cast<epoc::canvas_base *>(user->parent)->absolute_position();
        }

        evt.handle = user->get_client_handle();
    }

    void window_pointer_focus_walker::process_event_to_target_window(epoc::window *win, epoc::event &evt) {
        assert(win->type == epoc::window_kind::client);

        epoc::canvas_base *user = reinterpret_cast<epoc::canvas_base *>(win);
        // Stop, we found it!
        // Send it right now
        prepare_event_for_window(user, scr_coord_, evt);

        kernel_system *kern = win->client->get_ws().get_kernel_system();

        kern->lock();
        if ((evt.adv_pointer_evt_.evtype == epoc::event_type::button1down) && (user->flags & epoc::window::flags_allow_pointer_grab)) {
            grab_windows_[evt.adv_pointer_evt_.ptr_num] = user;
        }

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

        epoc::canvas_base *user = reinterpret_cast<epoc::canvas_base *>(win);

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

            const bool filter_drag = (evt.adv_pointer_evt_.evtype == epoc::event_type::drag) && (user->filter & epoc::pointer_filter_type::pointer_drag);
            const bool filter_move = (evt.adv_pointer_evt_.evtype == epoc::event_type::move) && (user->filter & epoc::pointer_filter_type::pointer_move);

            // Filter out events, assuming move event never exist (phone)
            // When you use touch on your phone, you drag your finger. Move your mouse (2023 correct:) is on PC (~~simply doesn't exist~~ is on mobile).
            if (filter_enter_exit || filter_drag || filter_move) {
                continue;
            }

            if (!sent_to_highest_z) {
                if (user->visible_region.contains(evt.adv_pointer_evt_.pos)) {
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

    bool window_pointer_focus_walker::deliver_to_grab_window(kernel_system *kern, epoc::event &evt) {
        const std::lock_guard<kernel_system> guard(*kern);

        auto grab_ite = grab_windows_.find(evt.adv_pointer_evt_.ptr_num);
        if (grab_ite == grab_windows_.end()) {
            return false;
        }

        epoc::canvas_base *grab_window = grab_ite->second;

        // A down with the grab still held means the matching up was lost; hit-test afresh.
        if (evt.adv_pointer_evt_.evtype == epoc::event_type::button1down) {
            grab_windows_.erase(grab_ite);
            return false;
        }

        if (evt.adv_pointer_evt_.evtype == epoc::event_type::button1up) {
            grab_windows_.erase(grab_ite);
        }

        prepare_event_for_window(grab_window, evt.adv_pointer_evt_.pos, evt);
        grab_window->queue_event(evt);

        return true;
    }

    void window_pointer_focus_walker::release_grab(epoc::window *win) {
        for (auto ite = grab_windows_.begin(); ite != grab_windows_.end();) {
            if (ite->second == win) {
                ite = grab_windows_.erase(ite);
            } else {
                ite++;
            }
        }
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

        kernel_system *kern = serv_->get_kernel_system();
        const std::lock_guard<kernel_system> guard(*kern);
        epoc::window_group *focus = serv_->get_focus();
        ntimer *timing = kern->get_ntimer();

        for (auto &evt : evts_) {
            if (focus) {
                evt.key_evt_.scancode = epoc::post_processing_scancode(static_cast<epoc::std_scan_code>(evt.key_evt_.scancode),
                    focus->scr->ui_rotation);
            }

            const auto previous_modifiers = translator_.modifiers();
            const auto translated = translator_.translate(evt);
            const auto changed_modifiers = previous_modifiers ^ translator_.modifiers();
            if (changed_modifiers) {
                for (auto &[uid, client] : serv_->clients) {
                    client->send_modifier_changed_events(changed_modifiers, translator_.modifiers());
                }
            }

            const bool dont_send_extra_key_event = !translated.has_value();
            const bool repeatable = key_event_translator::is_repeatable(evt.key_evt_.scancode);
            epoc::event extra_event = translated.value_or(evt);
            const auto the_code = epoc::map_scancode_to_keycode(static_cast<std_scan_code>(evt.key_evt_.scancode));
            const std::uint64_t data_for_repeatable = evt.key_evt_.scancode | (static_cast<std::uint64_t>(the_code) << 32);

            if ((evt.type == epoc::event_code::key_up) && repeatable) {
                if (!timing->unschedule_event(serv_->repeatable_event_, data_for_repeatable)) {
                    serv_->cancel_repeatable_list.insert(data_for_repeatable);
                }
            }
            if (!focus) {
                continue;
            }

            evt.handle = focus->get_client_handle();
            extra_event.handle = focus->get_client_handle();

            focus->queue_event(evt);

            if (!dont_send_extra_key_event) {
                // Give it a single key event also
                focus->queue_event(extra_event);

                if ((evt.type == epoc::event_code::key_down) && repeatable) {
                    timing->schedule_event(serv_->initial_repeat_delay_, serv_->repeatable_event_, data_for_repeatable);
                }
            }

            // Iterates through key capture requests and deliver those in needs.key_capture_request_queue &rqueue = key_capture_requests[extra_key_evt.key_evt_.code];
            window_server::key_capture_request_queue &rqueue = serv_->key_capture_requests[evt.key_evt_.code];

            for (auto ite = rqueue.end(); ite != rqueue.begin();) {
                --ite;
                // No need to deliver twice.
                if (ite->user->id == focus->id) {
                    break;
                }

                switch (ite->type_) {
                case epoc::event_key_capture_type::normal:
                    if (dont_send_extra_key_event) {
                        break;
                    }
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
