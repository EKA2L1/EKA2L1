/*
 * Copyright (c) 2023 EKA2L1 Team.
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

#include <dispatch/libraries/vg/gnuVG_emuutils.hh>
#include <dispatch/libraries/vg/consts.h>

#include <dispatch/dispatcher.h>
#include <system/epoc.h>
#include <kernel/kernel.h>
#include <dispatch/libraries/egl/def.h>

namespace gnuVG {
    Context *get_active_context(eka2l1::system *sys, GraphicState *graphic_state) {
        if (!sys) {
            return nullptr;
        }

        eka2l1::dispatch::dispatcher *dp = sys->get_dispatcher();
        if (!dp) {
            return nullptr;
        }

        eka2l1::kernel_system *kern = sys->get_kernel_system();
        if (!kern) {
            return nullptr;
        }

        eka2l1::kernel::thread *crr_thread = kern->crr_thread();
        if (!crr_thread) {
            return nullptr;
        }

        eka2l1::dispatch::egl_controller &controller = dp->get_egl_controller();
        eka2l1::kernel::uid crr_thread_uid = crr_thread->unique_id();

        eka2l1::dispatch::egl_context *context = controller.current_context(crr_thread_uid);
        if (!context) {
            controller.push_error(crr_thread_uid, VG_NO_CONTEXT_ERROR);
            return nullptr;
        }

        if (context->context_type() != eka2l1::dispatch::EGL_VG_CONTEXT) {
            controller.push_error(crr_thread_uid, VG_NO_CONTEXT_ERROR);
            return nullptr;
        }

        if (graphic_state != nullptr) {
            graphic_state->driver = sys->get_graphics_driver();
            graphic_state->shaderman = &controller.get_vg_shaderman();
        }

        return reinterpret_cast<Context*>(context);
    }
}