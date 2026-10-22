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

    static bool base_area_contains(epoc::canvas_base *win, const eka2l1::vec2 &pos) {
        if (!win->abs_rect.contains(pos)) {
            return false;
        }

        return !(win->flags & epoc::window::flag_shape_region) || win->shape_region.contains(pos);
    }

    // Descend a window group front to back, entering every visible client window that accepts the pointer.
    template <typename F>
    static epoc::canvas_base *descend_group(epoc::window *group, F &&accept) {
        epoc::canvas_base *result = nullptr;
        epoc::window *win = group->child;

        while (win) {
            if (win->type == epoc::window_kind::top_client) {
                win = win->child;
                continue;
            }

            if (win->type == epoc::window_kind::client) {
                epoc::canvas_base *canvas = reinterpret_cast<epoc::canvas_base *>(win);

                if (canvas->is_visible() && accept(canvas)) {
                    result = canvas;
                    win = win->child;
                    continue;
                }
            }

            win = win->sibling;
        }

        return result;
    }

    epoc::canvas_base *window_pointer_focus_walker::take_grab_window(const epoc::event &evt) {
        auto grab_ite = grab_windows_.find(evt.adv_pointer_evt_.ptr_num);
        if (grab_ite == grab_windows_.end()) {
            return nullptr;
        }

        epoc::canvas_base *grab_window = grab_ite->second;

        // A down with the grab still held means the matching up was lost; hit-test afresh.
        if (evt.adv_pointer_evt_.evtype == epoc::event_type::button1down) {
            grab_windows_.erase(grab_ite);
            return nullptr;
        }

        if (evt.adv_pointer_evt_.evtype == epoc::event_type::button1up) {
            grab_windows_.erase(grab_ite);
        }

        return grab_window;
    }

    void window_pointer_focus_walker::deliver(kernel_system *kern, epoc::window *first_group, epoc::window *focus, epoc::event &evt) {
        const std::lock_guard<kernel_system> guard(*kern);
        const eka2l1::vec2 pos = evt.adv_pointer_evt_.pos;

        // CWsRootWindow::PointerWindow: first find the window the pointer is really over, ignoring capture.
        epoc::canvas_base *actual = nullptr;
        epoc::window *actual_group = nullptr;

        for (epoc::window *group = first_group; group && !actual; group = group->sibling) {
            actual = descend_group(group, [&](epoc::canvas_base *win) {
                return base_area_contains(win, pos);
            });

            actual_group = actual ? group : nullptr;
        }

        epoc::canvas_base *target = take_grab_window(evt);

        if (!target) {
            // Then search again, letting capturing windows claim the pointer outside their area.
            const std::int32_t actual_priority = actual ? actual->pointer_capture_priority_ : 0;

            for (epoc::window *group = first_group; group && !target; group = group->sibling) {
                const bool same_group = (group == actual_group);

                target = descend_group(group, [&](epoc::canvas_base *win) {
                    if (base_area_contains(win, pos)) {
                        return true;
                    }

                    if (!(win->flags & epoc::window::flags_pointer_capture)) {
                        return false;
                    }

                    if (same_group) {
                        return win->pointer_capture_priority_ >= actual_priority;
                    }

                    return (win->flags & epoc::window::flags_pointer_capture_all_groups) || (!actual_group && (group == focus));
                });
            }

            if (!target) {
                return;
            }

            auto contain_area = target->scr->pointer_areas_.find(target->scr->crr_mode);
            if ((contain_area != target->scr->pointer_areas_.end()) && !contain_area->second.contains(pos)) {
                return;
            }

            if ((evt.adv_pointer_evt_.evtype == epoc::event_type::button1down) && (target->flags & epoc::window::flags_allow_pointer_grab)) {
                grab_windows_[evt.adv_pointer_evt_.ptr_num] = target;
            }
        }

        // WsPointer::ProcessEvent filters against the receiving window, grabbed or not.
        const bool filter_enter_exit = ((evt.type == epoc::event_code::touch_enter) || (evt.type == epoc::event_code::touch_exit))
            && (target->filter & epoc::pointer_filter_type::pointer_enter);

        const bool filter_drag = (evt.adv_pointer_evt_.evtype == epoc::event_type::drag) && (target->filter & epoc::pointer_filter_type::pointer_drag);
        const bool filter_move = (evt.adv_pointer_evt_.evtype == epoc::event_type::move) && (target->filter & epoc::pointer_filter_type::pointer_move);

        if (filter_enter_exit || filter_drag || filter_move) {
            return;
        }

        epoc::event drag_drop = evt;

        prepare_event_for_window(target, pos, evt);
        target->queue_event(evt);

        // WsPointer::ProcessPointerEvent: a drag-drop capture also reports the event to the window under the pointer.
        if ((target->flags & epoc::window::flags_pointer_capture_drag_drop) && actual) {
            drag_drop.type = epoc::event_code::drag_and_drop;
            prepare_event_for_window(actual, pos, drag_drop);
            actual->queue_event(drag_drop);
        }
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
