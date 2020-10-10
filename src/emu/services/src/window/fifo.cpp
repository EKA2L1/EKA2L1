/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <common/log.h>
#include <services/window/fifo.h>

#include <cassert>

namespace eka2l1::epoc {
    bool event_fifo::is_my_priority_really_high(epoc::event_code evt) {
        switch (evt) {
        case event_code::switch_off:
        case event_code::switch_on:
        case event_code::event_password:
            return true;

        default:
            return false;
        }
    }

    std::uint32_t event_fifo::queue_event(const event &evt) {
        const std::lock_guard<std::mutex> guard(lock_);

        if (q_.size() == maximum_element) {
            do_purge();
        }

        std::uint32_t result = queue_event_dont_care(evt);
        trigger_notification();

        return result;
    }

    // Symbian purges:
    // Pointer up/down pairs
    // Key messages
    // Key updown pairs
    //
    // Focus lost/gain pair
    // Not purge
    // Lone pointer ups
    // Lone focus lost/gain
    void event_fifo::do_purge() {
        for (size_t i = 0; i < q_.size(); i++) {
            switch (q_[i].evt.type) {
            case epoc::event_code::event_password:
                break;

            case epoc::event_code::null:
            case epoc::event_code::key:
            case epoc::event_code::touch_enter:
            case epoc::event_code::touch_exit: {
                q_.erase(q_.begin() + i);
                break;
            }

            case epoc::event_code::touch: {
                // TODO: implement logics in
                // https://github.com/SymbianSource/oss.FCL.sf.os.graphics/blob/ff133bc50e6158bfb08cc093b0f0055321dcde99/windowing/windowserver/nga/SERVER/EVQUEUE.CPP#L630
                // just purge it right now
                q_.erase(q_.begin() + i);
                break;
            }

            case epoc::event_code::focus_gained:
            case epoc::event_code::focus_lost: {
                if ((i + 1 < q_.size()) && ((q_[i + 1].evt.type == epoc::event_code::focus_gained) || (q_[i + 1].evt.type == epoc::event_code::focus_lost))) {
                    q_.erase(q_.begin() + i + 1);
                    q_.erase(q_.begin() + i);
                }

                break;
            }

            case epoc::event_code::switch_on: {
                if (i + 1 < q_.size() && (q_[i + 1].evt.type == epoc::event_code::switch_on)) {
                    q_.erase(q_.begin() + i);
                    break;
                }
            }

            default: {
                LOG_ERROR("Unhandled purge of event type: {}", static_cast<int>(q_[i].evt.type));
                assert(false);

                break;
            }
            }
        }
    }

    event event_fifo::get_event() {
        std::optional<event> evt = get_evt_opt();

        if (!evt) {
            // Create a null event
            return event(0, event_code::null);
        }

        return *evt;
    }

    std::uint32_t redraw_fifo::queue_event(void *owner, const redraw_event &evt, const std::uint16_t pri) {
        const std::lock_guard<std::mutex> guard(lock_);
        eka2l1::rect target_queue_rect(evt.top_left, evt.bottom_right);
        target_queue_rect.transform_from_symbian_rectangle();

        std::size_t limit = q_.size();

        for (std::size_t i = 0; i < limit; i++) {
            eka2l1::rect queued_rect(q_[i].evt.evt_.top_left, q_[i].evt.evt_.bottom_right);
            queued_rect.transform_from_symbian_rectangle();

            if ((q_[i].evt.evt_.handle == evt.handle) && target_queue_rect.contains(queued_rect)) {
                // The new redraw rect contains the old queued redraw rect. Remove it to avoid
                // unneccessary redraws.
                q_.erase(q_.begin() + i);
                limit--;
            }
        }
        
        redraw_event_full full_event;
        full_event.owner_ = owner;
        full_event.evt_ = evt;
        
        std::uint32_t id = queue_event_dont_care(full_event);
        q_.back().pri = pri;

        std::stable_sort(q_.begin(), q_.end(),
            [&](const fifo_element &e1, const fifo_element &e2) {
                return e1.pri < e2.pri;
            });

        // Queue a redraw won't directly trigger a notification.
        return id;
    }

    void redraw_fifo::remove_events(void *owner) {
        const std::lock_guard<std::mutex> guard(lock_);
        common::erase_elements(q_, [owner](fifo_element &elem) -> bool {
            return elem.evt.owner_ == owner;
        });
    }
}