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

#include <dispatch/dispatcher.h>
#include <dispatch/register.h>
#include <dispatch/screen.h>
#include <kernel/kernel.h>
#include <services/window/window.h>
#include <utils/event.h>
#include <utils/err.h>

#include <common/log.h>
#include <system/epoc.h>

namespace eka2l1::dispatch {
    dispatcher::dispatcher(kernel_system *kern, ntimer *timing)
        : winserv_(nullptr) {
        winserv_ = reinterpret_cast<eka2l1::window_server *>(kern->get_by_name<service::server>(
            eka2l1::get_winserv_name_by_epocver(kern->get_epoc_version())));

        audren_sema_ = std::make_unique<dsp_epoc_audren_sema>();

        // Set global variables
        timing_ = timing;
    }

    dispatcher::~dispatcher() {
        shutdown();
    }

    void dispatcher::resolve(eka2l1::system *sys, const std::uint32_t function_ord) {
        auto dispatch_find_result = dispatch::dispatch_funcs.find(function_ord);

        if (dispatch_find_result == dispatch::dispatch_funcs.end()) {
            LOG_ERROR(HLE_DISPATCHER, "Can't find dispatch function {}", function_ord);
            return;
        }

        dispatch_find_result->second(sys, sys->get_kernel_system()->crr_process(), sys->get_cpu());
    }

    void dispatcher::shutdown() {
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

    dsp_epoc_audren_sema *dispatcher::get_audren_sema() {
        return audren_sema_.get();
    }
}

namespace eka2l1::epoc {
    void dispatcher_do_resolve(eka2l1::system *sys, const std::uint32_t ordinal) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatcher->resolve(sys, ordinal);
    }

    void dispatcher_do_event_add(eka2l1::system *sys, epoc::raw_event &evt) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();

        switch (evt.type_) {
        case epoc::raw_event_type_redraw:
            dispatcher->update_all_screens(sys);
            break;

        default:
            LOG_WARN(HLE_DISPATCHER, "Unhandled raw event {}", static_cast<int>(evt.type_));
            break;
        }
    }
}