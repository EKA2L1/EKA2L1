/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <epoc/dispatch/dispatcher.h>
#include <epoc/dispatch/register.h>
#include <epoc/dispatch/screen.h>
#include <epoc/kernel.h>
#include <epoc/services/window/window.h>
#include <epoc/utils/err.h>

#include <common/log.h>

namespace eka2l1::dispatch {
    dispatcher::dispatcher()
        : winserv_(nullptr) {
    }

    void dispatcher::init(kernel_system *kern, ntimer *timing) {
        winserv_ = reinterpret_cast<eka2l1::window_server *>(kern->get_by_name<service::server>(eka2l1::WINDOW_SERVER_NAME));
        
        audio_nof_complete_evt_ = timing->register_event("DispatchAudio", [this, kern, timing](std::uint64_t userdata, std::uint64_t late) {  
            dsp_epoc_stream *stream = reinterpret_cast<dsp_epoc_stream*>(userdata);
            stream->deliver_audio_events(kern, timing);
            timing->schedule_event(500 - late, audio_nof_complete_evt_, userdata);
        });

        // Set global variables
        timing_ = timing;
    }

    void dispatcher::resolve(eka2l1::system *sys, const std::uint32_t function_ord) {
        auto dispatch_find_result = dispatch::dispatch_funcs.find(function_ord);

        if (dispatch_find_result == dispatch::dispatch_funcs.end()) {
            LOG_ERROR("Can't find dispatch function {}", function_ord);
            return;
        }

        dispatch_find_result->second.func(sys);
    }

    void dispatcher::shutdown() {
        // TODO:
    }

    void dispatcher::update_all_screens(eka2l1::system *sys) {
        epoc::screen *scr = winserv_->get_screens();

        while (scr != nullptr) {
            eka2l1::rect up_rect;
            up_rect.top = { 0, 0 };
            up_rect.size = scr->size();

            dispatch::update_screen(sys, 0, scr->number, 1, &up_rect);
            scr = scr->next;
        }
    }
}