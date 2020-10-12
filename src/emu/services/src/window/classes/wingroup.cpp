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

#include <services/window/classes/scrdvc.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <services/window/window.h>
#include <utils/sec.h>

#include <common/cvt.h>
#include <common/log.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    static epoc::security_policy key_capture_policy({ epoc::cap_sw_event });

    void window_group::lost_focus() {
        queue_event(epoc::event{ id, epoc::event_code::focus_lost });
    }

    void window_group::gain_focus() {
        queue_event(epoc::event{ id, epoc::event_code::focus_gained });
    }

    void window_group::receive_focus(service::ipc_context &context, ws_cmd &cmd) {
        flags &= ~flag_focus_receiveable;

        if (*reinterpret_cast<std::uint32_t *>(cmd.data_ptr)) {
            flags |= flag_focus_receiveable;

            LOG_TRACE("Request group {} to enable keyboard focus", common::ucs2_to_utf8(name));
        } else {
            LOG_TRACE("Request group {} to disable keyboard focus", common::ucs2_to_utf8(name));
        }

        scr->update_focus(&client->get_ws(), nullptr);
        context.complete(epoc::error_none);
    }

    window_group::window_group(window_server_client_ptr client, screen *scr, epoc::window *parent, const std::uint32_t client_handle)
        : window(client, scr, parent, window_kind::group) {
        // Create window group as child
        top = std::make_unique<window_top_user>(client, scr, this);
        child = top.get();

        set_client_handle(client_handle);
    }

    eka2l1::vec2 window_group::get_origin() {
        return { 0, 0 };
    }

    void window_group::queue_message_data(const std::uint8_t *data, const std::size_t data_size) {
        message_data data_vec;
        data_vec.resize(data_size);

        std::copy(data, data + data_size, data_vec.begin());
        msg_datas.push(std::move(data_vec));
    }

    void window_group::get_message_data(std::uint8_t *data, std::size_t &dest_size) {
        if (msg_datas.empty()) {
            dest_size = 0;
            return;
        }

        message_data data_vec = std::move(msg_datas.front());
        msg_datas.pop();

        dest_size = common::min<std::size_t>(data_vec.size(), dest_size);
        std::copy(data_vec.begin(), data_vec.begin() + dest_size, data);
    }
    
    void window_group::set_text_cursor(service::ipc_context &context, ws_cmd &cmd) {
        // Warn myself in the future!
        LOG_WARN("Set cursor text is mostly a stubbed now");

        ws_cmd_set_text_cursor *cmd_set = reinterpret_cast<decltype(cmd_set)>(cmd.data_ptr);
        auto window_user_to_set = reinterpret_cast<window_user *>(client->get_object(cmd_set->win));

        if (!window_user_to_set || (window_user_to_set->type != window_kind::client)) {
            LOG_ERROR("Window not found or not client kind to set text cursor");
            context.complete(epoc::error_not_found);
            return;
        }

        window_user_to_set->cursor_pos = cmd_set->pos + window_user_to_set->pos;
        context.complete(epoc::error_none);
    }

    void window_group::add_priority_key(service::ipc_context &context, ws_cmd &cmd) {
        LOG_TRACE("Add priortiy key stubbed");
        context.complete(epoc::error_none);
    }

    void window_group::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        bool result = execute_command_for_general_node(ctx, cmd);
        //LOG_TRACE("Window group op: {}", cmd.header.op);

        if (result) {
            return;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpEnableScreenChangeEvents: {
            epoc::event_screen_change_user evt;
            evt.user = this;

            client->add_event_notifier<epoc::event_screen_change_user>(evt);
            ctx.complete(epoc::error_none);

            break;
        }

        case EWsWinOpCaptureKey:
        case EWsWinOpCaptureKeyUpsAndDowns: {
            if (!ctx.satisfy(key_capture_policy)) {
                ctx.complete(epoc::error_permission_denied);
                break;
            }

            ws_cmd_capture_key *capture_key_cmd = reinterpret_cast<decltype(capture_key_cmd)>(cmd.data_ptr);

            epoc::event_capture_key_notifier capture_key_notify;

            capture_key_notify.user = this;
            capture_key_notify.keycode_ = capture_key_cmd->key;
            capture_key_notify.modifiers_ = capture_key_cmd->modifiers;
            capture_key_notify.modifiers_mask_ = capture_key_cmd->modifier_mask;
            capture_key_notify.type_ = (op == EWsWinOpCaptureKeyUpsAndDowns) ? epoc::event_key_capture_type::up_and_downs : epoc::event_key_capture_type::normal;
            capture_key_notify.pri_ = capture_key_cmd->priority;

            ctx.complete(client->add_event_notifier(capture_key_notify));

            break;
        }

        case EWsWinOpSetName: {
            auto name_re = ctx.get_argument_value<std::u16string>(remote_slot);

            if (!name_re) {
                ctx.complete(epoc::error_argument);
                break;
            }

            name = std::move(*name_re);
            ctx.complete(epoc::error_none);

            break;
        }                   

        case EWsWinOpName: {
            ctx.write_arg(reply_slot, name);
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpEnableGroupListChangeEvents:
        case EWsWinOpDisableScreenChangeEvents:
        case EWsWinOpDisableModifierChangedEvents:
        case EWsWinOpDisableErrorMessages:
        case EWsWinOpDisableOnEvents:
        case EWsWinOpDisableGroupChangeEvents:
        case EWsWinOpDisableKeyClick:
        case EWsWinOpDisableFocusChangeEvents:
        case EWsWinOpDisableGroupListChangeEvents:
        case EWsWinOpEnableOnEvents: {
            LOG_TRACE("Currently not support lock/unlock event for window server");
            ctx.complete(epoc::error_none);

            break;
        }

        case EWsWinOpReceiveFocus: {
            receive_focus(ctx, cmd);
            break;
        }

        case EWsWinOpSetTextCursor: {
            set_text_cursor(ctx, cmd);
            break;
        }

        case EWsWinOpOrdinalPosition: {
            ctx.complete(ordinal_position(true));
            break;
        }

        case EWsWinOpOrdinalPriority: {
            ctx.complete(priority);
            break;
        }

        case EWsWinOpAddPriorityKey:
            add_priority_key(ctx, cmd);
            break;

        default: {
            LOG_ERROR("Unimplemented window group opcode 0x{:X}!", cmd.header.op);
            break;
        }
        }
    }
}
