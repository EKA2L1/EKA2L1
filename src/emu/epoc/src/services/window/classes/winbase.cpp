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
#include <epoc/services/window/window.h>
#include <epoc/services/window/op.h>
#include <epoc/services/window/opheader.h>

namespace eka2l1::epoc {
    void window::queue_event(const epoc::event &evt) {
        client->queue_event(evt);
    }

    // I make this up myself
    std::uint16_t window::redraw_priority() {
        std::uint16_t pri;

        if (parent) {
            pri = parent->redraw_priority();
        }

        pri += (priority << 4) + secondary_priority;
        return pri;
    }

    void window::priority_updated() {
        for (auto &child : childs) {
            child->priority_updated();
        }
    }

    bool window::execute_command_for_general_node(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd cmd) {
        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpEnableModifierChangedEvents: {
            epoc::event_mod_notifier_user nof;
            nof.notifier = *reinterpret_cast<event_mod_notifier *>(cmd.data_ptr);
            nof.user = this;

            client->add_event_mod_notifier_user(nof);
            ctx.set_request_status(KErrNone);

            return true;
        }

        case EWsWinOpSetOrdinalPosition: {
            priority = *reinterpret_cast<int *>(cmd.data_ptr);
            priority_updated();

            ctx.set_request_status(KErrNone);

            return true;
        }

        case EWsWinOpSetOrdinalPositionPri: {
            ws_cmd_ordinal_pos_pri *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
            priority = info->pri1;
            secondary_priority = info->pri2;

            priority_updated();

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

            client->add_event_error_msg_user(nof);
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