/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <services/window/io.h>
#include <services/window/op.h>
#include <services/window/window.h>

#include <services/fbs/fbs.h>
#include <services/utils.h>
#include <services/window/classes/bitmap.h>
#include <services/window/classes/dsa.h>
#include <services/window/classes/gctx.h>
#include <services/window/classes/plugins/animdll.h>
#include <services/window/classes/plugins/clickdll.h>
#include <services/window/classes/plugins/graphics.h>
#include <services/window/classes/plugins/sprite.h>
#include <services/window/classes/scrdvc.h>
#include <services/window/classes/winbase.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/classes/wsobj.h>
#include <services/window/common.h>

#include <common/algorithm.h>
#include <common/armemitter.h>
#include <common/cvt.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/rgb.h>
#include <common/time.h>

#include <config/app_settings.h>
#include <config/config.h>

#include <utils/err.h>
#include <utils/event.h>

#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <system/devices.h>
#include <system/epoc.h>
#include <vfs/vfs.h>

#include <loader/rom.h>

#include <optional>
#include <string>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

namespace eka2l1::epoc {
    bool operator<(const event_capture_key_notifier &lhs, const event_capture_key_notifier &rhs) {
        return lhs.pri_ < rhs.pri_;
    }

    graphics_orientation number_to_orientation(int rot) {
        switch (rot) {
        case 0: {
            return graphics_orientation::normal;
        }

        case 90: {
            return graphics_orientation::rotated90;
        }

        case 180: {
            return graphics_orientation::rotated180;
        }

        case 270: {
            return graphics_orientation::rotated270;
        }

        default: {
            break;
        }
        }

        assert(false && "UNREACHABLE");
        return graphics_orientation::normal;
    }

    window_server_client::~window_server_client() {
        objects.clear();
    }

    void window_server_client::parse_command_buffer(service::ipc_context &ctx) {
        std::optional<std::string> dat = ctx.get_argument_value<std::string>(cmd_slot);

        if (!dat) {
            return;
        }

        char *beg = dat->data();
        char *end = dat->data() + dat->size();

        std::vector<ws_cmd> cmds;

        while (beg < end) {
            ws_cmd cmd;

            cmd.header = *reinterpret_cast<ws_cmd_header *>(beg);

            if (cmd.header.op & 0x8000) {
                cmd.header.op &= ~0x8000;
                cmd.obj_handle = *reinterpret_cast<std::uint32_t *>(beg + sizeof(ws_cmd_header));

                beg += sizeof(ws_cmd_header) + sizeof(cmd.obj_handle);
            } else {
                beg += sizeof(ws_cmd_header);
            }

            cmd.data_ptr = reinterpret_cast<void *>(beg);
            beg += cmd.header.cmd_len;

            cmds.push_back(std::move(cmd));
        }

        execute_commands(ctx, std::move(cmds));
    }

    window_server_client::window_server_client(service::session *guest_session, kernel::thread *own_thread, epoc::version ver)
        : guest_session(guest_session)
        , client_thread(own_thread)
        , last_obj(nullptr)
        , cli_version(ver)
        , primary_device(nullptr)
        , uid_counter(0) {
    }

    void window_server_client::execute_commands(service::ipc_context &ctx, std::vector<ws_cmd> cmds) {
        for (auto &cmd : cmds) {
            if (cmd.obj_handle == guest_session->unique_id()) {
                if (last_obj) {
                    last_obj->on_command_batch_done(ctx);
                    last_obj = nullptr;
                }

                execute_command(ctx, cmd);
            } else {
                if (auto obj = get_object(cmd.obj_handle)) {
                    if (last_obj != obj) {
                        if (last_obj != nullptr) {
                            last_obj->on_command_batch_done(ctx);
                        }

                        last_obj = obj;
                    }

                    if (obj->execute_command(ctx, cmd)) {
                        // The command batch is silently flushed...
                        last_obj = nullptr;
                    }
                }
            }
        }

        if (last_obj) {
            last_obj->on_command_batch_done(ctx);
            last_obj = nullptr;
        }
    }

    std::uint32_t window_server_client::queue_redraw(epoc::canvas_base *user, const eka2l1::rect &redraw_rect) {
        return redraws.queue_event(user, epoc::redraw_event{ user->get_client_handle(), redraw_rect.top, redraw_rect.size + redraw_rect.top },
            user->redraw_priority());
    }

    std::uint32_t window_server_client::add_object(window_client_obj_ptr &obj) {
        auto free_slot = std::find(objects.begin(), objects.end(), nullptr);

        if (free_slot != objects.end()) {
            *free_slot = std::move(obj);
            return ((std::distance(objects.begin(), free_slot) + 1) & 0xFFFF) | (++uid_counter << 16);
        }

        objects.push_back(std::move(obj));
        return (objects.size() & 0xFFFF) | (++uid_counter << 16);
    }

    epoc::window_client_obj *window_server_client::get_object(const std::uint32_t handle) {
        const std::uint32_t idx = handle & 0xFFFF;

        if (idx > objects.size() || idx == 0) {
            LOG_WARN(SERVICE_WINDOW, "Object handle is invalid {}", handle);
            return nullptr;
        }

        return objects[idx - 1].get();
    }

    bool window_server_client::delete_object(const std::uint32_t handle) {
        const std::uint32_t idx = handle & 0xFFFF;

        if (idx > objects.size() || idx == 0) {
            LOG_WARN(SERVICE_WINDOW, "Object handle is invalid {}", handle);
            return false;
        }

        objects[idx - 1].reset();
        return true;
    }

    void window_server_client::create_screen_device(service::ipc_context &ctx, ws_cmd &cmd) {
        LOG_INFO(SERVICE_WINDOW, "Create screen device.");

        ws_cmd_screen_device_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);

        // Get screen object
        epoc::screen *target_screen = get_ws().get_screen((cmd.header.cmd_len == 0) ? 0 : header->num_screen);

        if (!target_screen) {
            LOG_ERROR(SERVICE_WINDOW, "Can't find screen object with number {}", header->num_screen);
            ctx.complete(epoc::error_not_found);
            return;
        }

        window_client_obj_ptr device = std::make_unique<epoc::screen_device>(this, target_screen);

        if (!primary_device) {
            primary_device = reinterpret_cast<epoc::screen_device *>(device.get());
        }

        ctx.complete(add_object(device));
    }

    void window_server_client::create_dsa(service::ipc_context &ctx, ws_cmd &cmd) {
        window_client_obj_ptr dsa_inst = std::make_unique<epoc::dsa>(this);
        if (!dsa_inst) {
            ctx.complete(epoc::error_general);
            return;
        }

        ctx.complete(add_object(dsa_inst));
    }

    void window_server_client::restore_hotkey(service::ipc_context &ctx, ws_cmd &cmd) {
        THotKey key = *reinterpret_cast<THotKey *>(cmd.data_ptr);

        LOG_WARN(SERVICE_WINDOW, "Unknown restore key op.");
    }

    void window_server_client::create_window_group(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_window_group_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);
        int device_handle = header->screen_device_handle;

        epoc::screen_device *device_ptr = nullptr;
        epoc::screen *target_screen = get_ws().get_current_focus_screen();
        epoc::window *parent_group = target_screen->root.get();

        if (cmd.header.cmd_len > 8) {
            if (device_handle <= 0) {
                device_ptr = primary_device;
            } else {
                device_ptr = reinterpret_cast<epoc::screen_device *>(get_object(device_handle));
            }

            if (!device_ptr) {
                device_ptr = primary_device;
            }

            if (device_ptr) {
                target_screen = device_ptr->scr;
            }

            parent_group = reinterpret_cast<epoc::window *>(get_object(header->parent_id));

            if (!parent_group) {
                LOG_WARN(SERVICE_WINDOW, "Unable to find parent for new group with ID = 0x{:x}. Use root", header->parent_id);
                parent_group = target_screen->root.get();
            }
        }

        window_client_obj_ptr group = std::make_unique<epoc::window_group>(this, target_screen, parent_group, header->client_handle);
        epoc::window_group *group_casted = reinterpret_cast<epoc::window_group *>(group.get());

        // If no window group is being focused on the screen, we force the screen to receive this window as focus
        // Else rely on the focus flag.
        if (!target_screen->focus || (header->focus)) {
            group_casted->set_receive_focus(true);
            target_screen->update_focus(&get_ws(), nullptr);
        }

        // Give it a nice name.
        // We can give it name with id, but too much hassle
        group_casted->name = common::utf8_to_ucs2(fmt::format("WindowGroup{:X}", header->client_handle));

        std::uint32_t id = add_object(group);
        ctx.complete(id);
    }

    void window_server_client::create_window_base(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_window_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);
        epoc::window *parent = reinterpret_cast<epoc::window *>(get_object(header->parent));

        if (!parent) {
            LOG_WARN(SERVICE_WINDOW, "Unable to find parent for new window with ID = 0x{:x}. Use root", header->parent);
            ctx.complete(epoc::error_argument);
            return;
        }

        if (parent->type != window_kind::group && parent->type != window_kind::client) {
            LOG_ERROR(SERVICE_WINDOW, "The parent of window user must be a group or another user!");
            ctx.complete(epoc::error_argument);
            return;
        }

        kernel_system *kern = get_ws().get_kernel_system();
        epoc::display_mode disp = (header->dmode == epoc::display_mode::none) ? parent->scr->disp_mode : header->dmode;

        if (kern->get_epoc_version() >= epocver::epoc94) {
            // It seems that the mode is fixed to screen mode starting from s60v5. like a migration...?
            disp = parent->scr->disp_mode;
        }

        // We have to be child's parent child, which is top user.
        window_client_obj_ptr win = nullptr;
        switch (header->win_type) {
        case epoc::window_type::redraw:
            win = std::make_unique<epoc::redraw_msg_canvas>(this, parent->scr, (parent->type == window_kind::group) ? parent->child : parent,
                disp, header->client_handle);

            break;

        case epoc::window_type::blank:
            win = std::make_unique<epoc::blank_canvas>(this, parent->scr, (parent->type == window_kind::group) ? parent->child : parent,
                disp, header->client_handle);

            break;

        case epoc::window_type::backed_up:
            win = std::make_unique<epoc::bitmap_backed_canvas>(this, parent->scr, (parent->type == window_kind::group) ? parent->child : parent,
                disp, header->client_handle);

            break;

        default:
            LOG_ERROR(SERVICE_WINDOW, "Unimplemented window type {}!", static_cast<int>(header->win_type));
            ctx.complete(epoc::error_not_supported);
            return;
        }

        ctx.complete(add_object(win));
    }

    void window_server_client::create_graphic_context(service::ipc_context &ctx, ws_cmd &cmd) {
        window_client_obj_ptr gcontext = std::make_unique<epoc::graphic_context>(this);
        ctx.complete(add_object(gcontext));
    }

    void window_server_client::create_sprite(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_create_sprite_header *sprite_header = reinterpret_cast<decltype(sprite_header)>(cmd.data_ptr);
        epoc::window *win = reinterpret_cast<epoc::window *>(get_object(sprite_header->window_handle));

        if (!win) {
            LOG_WARN(SERVICE_WINDOW, "Window handle is invalid! Abort");
            ctx.complete(epoc::error_argument);
            return;
        }

        window_client_obj_ptr spr = std::make_unique<epoc::sprite>(this, win->scr, win, sprite_header->base_pos);
        ctx.complete(add_object(spr));
    }

    void window_server_client::create_graphic(service::ipc_context &ctx, ws_cmd &cmd) {
        LOG_TRACE(SERVICE_WINDOW, "Create graphic drawer stubbed!");

        window_client_obj_ptr drawer = std::make_unique<epoc::graphics_piece>(this, nullptr);
        ctx.complete(add_object(drawer));
    }

    void window_server_client::create_anim_dll(service::ipc_context &ctx, ws_cmd &cmd) {
        const int dll_name_length = *reinterpret_cast<int *>(cmd.data_ptr);
        const char16_t *dll_name_ptr = reinterpret_cast<char16_t *>(reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + sizeof(int));

        const std::u16string dll_name(dll_name_ptr, dll_name_length);

        LOG_TRACE(SERVICE_WINDOW, "Create ANIMDLL for {}, stubbed object", common::ucs2_to_utf8(dll_name));

        window_client_obj_ptr animdll = std::make_unique<epoc::anim_dll>(this, nullptr);
        ctx.complete(add_object(animdll));
    }

    void window_server_client::create_click_dll(service::ipc_context &ctx, ws_cmd &cmd) {
        LOG_TRACE(SERVICE_WINDOW, "Create CLICKDLL (button click sound plugin), stubbed object");

        window_client_obj_ptr clickdll = std::make_unique<epoc::click_dll>(this, nullptr);
        ctx.complete(add_object(clickdll));
    }

    void window_server_client::create_wsbmp(service::ipc_context &ctx, ws_cmd &cmd) {
        const std::uint32_t bmp_handle = *reinterpret_cast<const std::uint32_t *>(cmd.data_ptr);
        fbs_server *serv = get_ws().get_fbs_server();

        // Try to get the fbsbitmap
        fbsbitmap *bmp = serv->get<fbsbitmap>(bmp_handle);
        epoc::bitwise_bitmap *bw_bmp = nullptr;

        if (bmp) {
            bw_bmp = bmp->bitmap_;
        } else {
            memory_system *mem = get_ws().get_kernel_system()->get_memory_system();
            bw_bmp = eka2l1::ptr<epoc::bitwise_bitmap>(bmp_handle).get(mem);
        }

        if (!bw_bmp) {
            ctx.complete(epoc::error_bad_handle);
            return;
        }

        window_client_obj_ptr wsbmpobj = std::make_unique<epoc::wsbitmap>(this, bw_bmp, bmp);
        ctx.complete(add_object(wsbmpobj));
    }

    void window_server_client::get_window_group_list(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_window_group_list *list_req = reinterpret_cast<decltype(list_req)>(cmd.data_ptr);

        std::vector<std::uint8_t> ids;
        std::uint32_t total = 0;

        const int screen = (cmd.header.cmd_len == 8) ? 0 : list_req->screen_num;
        const int accept_priority = ((cmd.header.op == ws_cl_op_window_group_list_all_priorities) || (cmd.header.op == ws_cl_op_window_group_list_and_chain_all_priorities)) ? -1 : list_req->priority;

        if (cmd.header.op == ws_cl_op_window_group_list || cmd.header.op == ws_cl_op_window_group_list_all_priorities) {
            ids.resize(list_req->count * sizeof(std::uint32_t));
            total = get_ws().get_window_group_list(reinterpret_cast<std::uint32_t *>(&ids[0]), list_req->count,
                accept_priority, screen);
        } else {
            ids.resize(list_req->count * sizeof(epoc::window_group_chain_info));
            total = get_ws().get_window_group_list_and_chain(reinterpret_cast<epoc::window_group_chain_info *>(&ids[0]),
                list_req->count, accept_priority, screen);
        }

        ctx.write_data_to_descriptor_argument(reply_slot, reinterpret_cast<std::uint8_t *>(&ids[0]), static_cast<std::uint32_t>(ids.size()));
        ctx.complete(static_cast<int>(total));
    }

    void window_server_client::get_number_of_window_groups(service::ipc_context &ctx, ws_cmd &cmd) {
        ctx.complete(static_cast<int>(get_ws().get_total_window_groups(
            cmd.header.op == ws_cl_op_num_window_groups ? *static_cast<int *>(cmd.data_ptr) : -1)));
    }

    void window_server_client::send_event_to_window_group(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_send_event_to_window_group evt = *reinterpret_cast<ws_cmd_send_event_to_window_group *>(cmd.data_ptr);
        epoc::window_group *group = get_ws().get_group_from_id(evt.id);

        if (!group) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        get_ws().send_event_to_window_group(group, evt.evt);
        ctx.complete(epoc::error_none);
    }

    void window_server_client::send_event_to_all_window_groups(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_send_event_to_window_group evt = *reinterpret_cast<ws_cmd_send_event_to_window_group *>(cmd.data_ptr);
        get_ws().send_event_to_window_groups(evt.evt);

        ctx.complete(epoc::error_none);
    }

    void window_server_client::send_message_to_window_group(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_send_message_to_window_group *msg = reinterpret_cast<decltype(msg)>(cmd.data_ptr);
        epoc::window_group *group = get_ws().get_group_from_id(msg->id_or_priority);

        if (!group) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        kernel::process *caller_pr = ctx.msg->own_thr->owning_process();

        epoc::desc8 *msg_data_des = msg->data.get(caller_pr);
        std::uint8_t *msg_data = reinterpret_cast<std::uint8_t *>(msg_data_des->get_pointer(caller_pr));

        if (!msg_data) {
            ctx.complete(epoc::error_argument);
            return;
        }

        group->queue_message_data(msg_data, msg->data_length);

        epoc::event evt(group->get_client_handle(), epoc::event_code::event_messages_ready);

        evt.time = ctx.sys->get_kernel_system()->home_time();
        evt.msg_ready_evt_.window_group_id = group->id;
        evt.msg_ready_evt_.message_uid = msg->uid;
        evt.msg_ready_evt_.message_parameters_size = msg->data_length;

        group->client->queue_event(evt);
        ctx.complete(epoc::error_none);
    }

    void window_server_client::fetch_message(service::ipc_context &ctx, ws_cmd &cmd) {
        const std::uint32_t group_id = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        epoc::window_group *group = get_ws().get_group_from_id(group_id);

        if (!group) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        std::size_t dest_size = ctx.get_argument_max_data_size(reply_slot);
        std::uint8_t *dest_ptr = ctx.get_descriptor_argument_ptr(reply_slot);

        if (!dest_ptr) {
            ctx.complete(epoc::error_argument);
            return;
        }

        group->get_message_data(dest_ptr, dest_size);

        ctx.set_descriptor_argument_length(reply_slot, static_cast<std::uint32_t>(dest_size));
        ctx.complete(epoc::error_none);
    }

    void window_server_client::find_window_group_id(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_find_window_group_identifier *find_info = reinterpret_cast<decltype(find_info)>(cmd.data_ptr);
        epoc::window_group *group = nullptr;

        if (find_info->previous_id) {
            // Find our lost sibling!!!!! Bring him to me....
            group = get_ws().get_group_from_id(find_info->previous_id);

            if (!group) {
                LOG_ERROR(SERVICE_WINDOW, "Previous group sibling not found with id {}", find_info->previous_id);
                ctx.complete(epoc::error_not_found);
                return;
            }

            // She's sweet but sibling...
            // A little sibling
            group = reinterpret_cast<epoc::window_group *>(group->sibling);
        } else {
            group = get_ws().get_starting_group();
        }

        const char16_t *win_group_name_ptr = reinterpret_cast<char16_t *>(find_info + 1);
        const std::u16string win_group_name(win_group_name_ptr, find_info->length);

        for (; group; group = reinterpret_cast<epoc::window_group *>(group->sibling)) {
            // Whole string should be matched => from position 0
            if (common::match_wildcard_in_string(
                common::ucs2_to_wstr(group->name.substr(find_info->offset)),
                common::ucs2_to_wstr(win_group_name), true) == 0) {
                ctx.complete(group->id);
                return;
            }
        }

        ctx.complete(epoc::error_not_found);
    }

    void window_server_client::find_window_group_id_thread(service::ipc_context &ctx, ws_cmd &cmd) {
        std::uint32_t prev_id = 0;
        kernel::uid thr_id = 0;

        kernel_system *kern = ctx.sys->get_kernel_system();

        if (kern->is_eka1()) {
            ws_cmd_find_window_group_identifier_thread_eka1 *find_info = reinterpret_cast<decltype(find_info)>(
                cmd.data_ptr);

            prev_id = find_info->previous_id;
            thr_id = find_info->thread_id;
        } else {
            ws_cmd_find_window_group_identifier_thread *find_info = reinterpret_cast<decltype(find_info)>(
                cmd.data_ptr);

            prev_id = find_info->previous_id;
            thr_id = find_info->thread_id;
        }

        epoc::window_group *group = nullptr;

        if (prev_id) {
            group = get_ws().get_group_from_id(prev_id);

            if (!group) {
                LOG_ERROR(SERVICE_WINDOW, "Previous group sibling not found with id {}", prev_id);
                ctx.complete(epoc::error_not_found);
                return;
            }

            group = reinterpret_cast<epoc::window_group *>(group->sibling);
        } else {
            group = get_ws().get_starting_group();
        }

        for (; group; group = reinterpret_cast<epoc::window_group *>(group->sibling)) {
            if (group->client->get_client()->unique_id() == thr_id) {
                ctx.complete(group->id);
                return;
            }
        }

        ctx.complete(epoc::error_not_found);
    }

    void window_server_client::set_pointer_cursor_mode(service::ipc_context &ctx, ws_cmd &cmd) {
        // TODO: Check errors
        if (get_ws().get_focus() && get_ws().get_focus()->client == this) {
            get_ws().cursor_mode() = *reinterpret_cast<epoc::pointer_cursor_mode *>(cmd.data_ptr);
            ctx.complete(epoc::error_none);
            return;
        }

        ctx.complete(epoc::error_permission_denied);
    }

    void window_server_client::get_pointer_cursor_mode(service::ipc_context &ctx, ws_cmd &cmd) {
        ctx.complete(static_cast<int>(get_ws().cursor_mode()));
    }

    void window_server_client::get_window_group_client_thread_id(service::ipc_context &ctx, ws_cmd &cmd) {
        std::uint32_t group_id = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        epoc::window_group *win = get_ws().get_group_from_id(group_id);

        if (!win || (win->type != window_kind::group)) {
            LOG_TRACE(SERVICE_WINDOW, "Can't find group with id {}", group_id);
            ctx.complete(epoc::error_not_found);
            return;
        }

        const kernel::uid thr_id = win->client->get_client()->unique_id();
        kernel_system *kern = ctx.sys->get_kernel_system();

        if (kern->is_eka1()) {
            ctx.write_data_to_descriptor_argument<kernel::uid_eka1>(reply_slot, static_cast<kernel::uid_eka1>(thr_id));
        } else {
            ctx.write_data_to_descriptor_argument<kernel::uid>(reply_slot, thr_id);
        }

        ctx.complete(epoc::error_none);
    }

    void window_server_client::get_window_group_ordinal_priority(service::ipc_context &ctx, ws_cmd &cmd) {
        std::uint32_t group_id = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        epoc::window_group *win = get_ws().get_group_from_id(group_id);

        if (!win || (win->type != window_kind::group)) {
            LOG_TRACE(SERVICE_WINDOW, "Can't find group with id {}", group_id);
            ctx.complete(epoc::error_not_found);
            return;
        }

        ctx.complete(win->priority);
    }

    void window_server_client::get_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        auto evt = redraws.get_evt_opt();

        if (!evt) {
            // Report back a null redraw event
            epoc::redraw_event evt;
            std::memset(&evt, 0, sizeof(epoc::redraw_event));

            ctx.write_data_to_descriptor_argument<epoc::redraw_event>(reply_slot, evt);
        } else {
            ctx.write_data_to_descriptor_argument<epoc::redraw_event>(reply_slot, evt->evt_);
        }

        ctx.complete(epoc::error_none);
    }

    void window_server_client::get_event(service::ipc_context &ctx, ws_cmd &cmd) {
        auto evt = events.get_event();

        // Allow the context to shrink if needed, since the struct certainly got larger as Symbian
        // grows. S^3 has advance pointer struct which along takes 56 bytes buffer.
        ctx.write_data_to_descriptor_argument<epoc::event>(reply_slot, evt, nullptr, true);
        ctx.complete(epoc::error_none);
    }

    void window_server_client::get_focus_window_group(service::ipc_context &ctx, ws_cmd &cmd) {
        // TODO: Epoc < 9
        if (cmd.header.cmd_len == 0) {
            ctx.complete(get_ws().get_current_focus_screen()->focus->id);
            return;
        }

        int screen_num = *reinterpret_cast<int *>(cmd.data_ptr);
        epoc::screen *scr = get_ws().get_screen(screen_num);

        if (!scr) {
            LOG_ERROR(SERVICE_WINDOW, "Invalid screen number {}", screen_num);
            ctx.complete(epoc::error_argument);
            return;
        }

        ctx.complete(scr->focus->id);
    }

    void window_server_client::get_window_group_name_from_id(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_get_window_group_name_from_id *find_info = reinterpret_cast<decltype(find_info)>(cmd.data_ptr);
        epoc::window_group *group = get_ws().get_group_from_id(find_info->id);

        if (!group || group->type != window_kind::group) {
            ctx.complete(epoc::error_argument);
            return;
        }

        if (group->name.length() == 0) {
            ctx.complete(epoc::error_not_ready);
            return;
        }

        std::uint32_t len_to_write = std::min<std::uint32_t>(static_cast<std::uint32_t>(find_info->max_len),
            static_cast<std::uint32_t>(group->name.length()));

        const std::u16string to_write = group->name.substr(0, len_to_write);

        ctx.write_arg(reply_slot, to_write);
        ctx.complete(epoc::error_none);
    }

    struct window_clear_store_walker : public epoc::window_tree_walker {
        bool do_it(epoc::window *win) {
            if (win->type == window_kind::group) {
                win->client->trigger_redraw();
            }

            if (win->type == window_kind::client) {
                epoc::canvas_base *user = reinterpret_cast<epoc::canvas_base *>(win);

                if (user->win_type == epoc::window_type::redraw) {
                    epoc::redraw_msg_canvas *fm_user = reinterpret_cast<epoc::redraw_msg_canvas*>(win);
                    fm_user->clear_redraw_store();
                }
            }

            return false;
        }
    };

    void window_server_client::clear_all_redraw_stores(service::ipc_context &ctx, ws_cmd &cmd) {
        // Clear all stored drawing commands. We have none, but keep this here if we ever did.
        // Send redraws commands to all client window
        window_clear_store_walker walker;

        for (epoc::screen *scr = get_ws().screens; scr; scr = scr->next) {
            scr->root->walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children_and_previous_siblings);
        }

        ctx.complete(epoc::error_none);
    }

    void window_server_client::set_window_group_ordinal_position(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_set_window_group_ordinal_position *set = reinterpret_cast<decltype(set)>(cmd.data_ptr);

        window_server &serv = get_ws();
        epoc::window_group *group = serv.get_group_from_id(set->identifier);

        if (!group || group->type != window_kind::group) {
            ctx.complete(epoc::error_argument);
            return;
        }

        group->set_position(set->ord_pos);
        ctx.complete(epoc::error_none);
    }

    struct def_mode_max_num_colors {
        std::int32_t num_color;
        std::int32_t num_grey;
        epoc::display_mode disp_mode;
    };

    void window_server_client::get_def_mode_max_num_colors(service::ipc_context &ctx, ws_cmd &cmd) {
        const int screen_num = *reinterpret_cast<int *>(cmd.data_ptr);
        epoc::screen *scr = get_ws().get_screen(screen_num);

        if (!scr) {
            LOG_ERROR(SERVICE_WINDOW, "Can't find screen object with number {}, using the default screen", screen_num);
            scr = get_ws().get_current_focus_screen();
        }

        def_mode_max_num_colors result;
        scr->get_max_num_colors(result.num_color, result.num_grey);
        result.disp_mode = scr->disp_mode;

        ctx.write_data_to_descriptor_argument<def_mode_max_num_colors>(reply_slot, result);
        ctx.complete(epoc::error_none);
    }

    void window_server_client::get_color_mode_list(service::ipc_context &ctx, ws_cmd &cmd) {
        std::int32_t screen_num = *reinterpret_cast<std::int32_t *>(cmd.data_ptr);
        epoc::screen *scr = get_ws().get_screen(screen_num);

        if (!scr) {
            LOG_ERROR(SERVICE_WINDOW, "Can't find requested screen {}, use focus one", screen_num);
            scr = get_ws().get_current_focus_screen();
        }

        ctx.complete(1 << (static_cast<std::uint8_t>(scr->disp_mode) - 1));
    }

    void window_server_client::set_pointer_area(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_set_pointer_cursor_area *area_info = reinterpret_cast<ws_cmd_set_pointer_cursor_area *>(cmd.data_ptr);
        area_info->pointer_area.transform_from_symbian_rectangle();

        // Set for the default screen
        epoc::screen *scr = get_ws().get_screen(0);
        assert(scr);

        scr->pointer_areas_[area_info->mode] = area_info->pointer_area;

        ctx.complete(epoc::error_none);
    }

    void window_server_client::set_pointer_cursor_position(service::ipc_context &ctx, ws_cmd &cmd) {
        eka2l1::vec2 *pos = reinterpret_cast<eka2l1::vec2 *>(cmd.data_ptr);
        // Set for the default screen
        epoc::screen *scr = get_ws().get_screen(0);
        assert(scr);

        scr->pointer_cursor_pos_ = *pos;
        ctx.complete(epoc::error_none);
    }

    void window_server_client::get_number_of_screen(service::ipc_context &ctx, ws_cmd &cmd) {
        // We want to get the number of screen currently connected to the emulator
        epoc::screen *scr = get_ws().get_screens();
        std::uint32_t connected_count = 0;

        while (scr) {
            const eka2l1::vec2 size = scr->size();
            if ((size.x != -1) && (size.y != -1)) {
                // The screen is connected and has definite size.
                connected_count++;
            }

            scr = scr->next;
        }

        ctx.complete(connected_count);
    }

    void window_server_client::get_focus_screen(service::ipc_context &ctx, ws_cmd &cmd) {
        ctx.complete(get_ws().focus_screen_->number);
    }

    void window_server_client::get_ready(service::ipc_context &ctx, ws_cmd *cmd, const event_listener_type list_type) {
        epoc::notify_info info;
        info.requester = ctx.msg->own_thr;

        bool should_finish = false;

        if (cmd && cmd->header.cmd_len >= sizeof(address)) {
            info.sts = *reinterpret_cast<address *>(cmd->data_ptr);
            should_finish = true;
        } else {
            info.sts = ctx.msg->request_sts;
        }

        switch (list_type) {
        case event_listener_type_redraw:
            add_redraw_listener(info);
            break;

        case event_listener_type_event:
            add_event_listener(info);
            break;

        case event_listener_type_priority_key:
            priority_keys.set_listener(info);
            break;
        }

        if (should_finish) {
            ctx.complete(epoc::error_none);
        }
    }

    void window_server_client::add_raw_event(service::ipc_context &ctx, ws_cmd &cmd) {
        epoc::raw_event evt;

        if (cmd.header.cmd_len > sizeof(epoc::raw_event_eka1)) {
            evt = *reinterpret_cast<epoc::raw_event *>(cmd.data_ptr);
        } else {
            epoc::raw_event_eka1 evt_eka1;
            evt_eka1 = *reinterpret_cast<epoc::raw_event_eka1 *>(cmd.data_ptr);

            evt.from_eka1_event(evt_eka1);
        }

        LOG_TRACE(SERVICE_WINDOW, "Add raw event stubbed, event type {}", evt.type_);
        ctx.complete(epoc::error_none);
    }

    void window_server_client::set_keyboard_repeat_rate(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_keyboard_repeat_rate *repeat_rate = reinterpret_cast<ws_cmd_keyboard_repeat_rate *>(cmd.data_ptr);
        get_ws().set_keyboard_repeat_rate(repeat_rate->initial_time, repeat_rate->next_time);

        ctx.complete(epoc::error_none);
    }

    void window_server_client::get_keyboard_repeat_rate(service::ipc_context &ctx, ws_cmd &cmd) {
        std::uint64_t initial_time = 0;
        std::uint64_t next_time = 0;

        get_ws().get_keyboard_repeat_rate(initial_time, next_time);

        struct keyboard_repeat_rate_return_struct {
            std::uint32_t initial_32_;
            std::uint32_t next_32_;
        } value_return;

        value_return.initial_32_ = static_cast<std::uint32_t>(initial_time);
        value_return.next_32_ = static_cast<std::uint32_t>(next_time);

        ctx.write_data_to_descriptor_argument<keyboard_repeat_rate_return_struct>(reply_slot,
            value_return);
        ctx.complete(epoc::error_none);
    }

    void window_server_client::event_ready_cancel(service::ipc_context &ctx, ws_cmd &cmd) {
        events.cancel_listener();
        ctx.complete(epoc::error_none);
    }

    void window_server_client::redraw_ready_cancel(service::ipc_context &ctx, ws_cmd &cmd) {
        redraws.cancel_listener();
        ctx.complete(epoc::error_none);
    }

    // This handle both sync and async
    void window_server_client::execute_command(service::ipc_context &ctx, ws_cmd cmd) {
        // LOG_TRACE(SERVICE_WINDOW, "Window client op: {}", (int)cmd.header.op);
        epoc::version cli_ver = client_version();

        // Patching out user opcode.
        if (cli_ver.major == 1 && cli_ver.minor == 0) {
            if (cli_ver.build <= WS_OLDARCH_VER) {
                // Skip start and end custom text cursor, they does not exist
                if (cmd.header.op >= ws_cl_op_start_custom_text_cursor) {
                    cmd.header.op += 2;
                }
            }
        }

        switch (cmd.header.op) {
        // Gets the total number of window groups with specified priority currently running
        // in the window server.
        case ws_cl_op_num_window_groups:
        case ws_cl_op_num_groups_all_priorities: {
            get_number_of_window_groups(ctx, cmd);
            break;
        }

        case ws_cl_op_get_window_group_ordinal_priority:
            get_window_group_ordinal_priority(ctx, cmd);
            break;

        case ws_cl_op_send_event_to_window_group: {
            send_event_to_window_group(ctx, cmd);
            break;
        }

        case ws_cl_op_send_event_to_all_window_groups: {
            send_event_to_all_window_groups(ctx, cmd);
            break;
        }

        case ws_cl_op_send_message_to_window_group: {
            send_message_to_window_group(ctx, cmd);
            break;
        }

        case ws_cl_op_fetch_message:
            fetch_message(ctx, cmd);
            break;

        case ws_cl_op_purge_pointer_events:
            LOG_TRACE(SERVICE_WINDOW, "Purge pointer events stubbed");
            ctx.complete(epoc::error_none);
            break;

        case ws_cl_op_compute_mode: {
            LOG_TRACE(SERVICE_WINDOW, "Setting compute mode not supported, instead stubbed");
            ctx.complete(epoc::error_none);

            break;
        }

        case ws_cl_op_set_pointer_cursor_mode: {
            set_pointer_cursor_mode(ctx, cmd);
            break;
        }

        case ws_cl_op_pointer_cursor_mode:
            get_pointer_cursor_mode(ctx, cmd);
            break;

        case ws_cl_op_get_window_group_client_thread_id: {
            get_window_group_client_thread_id(ctx, cmd);
            break;
        }

        case ws_cl_op_get_redraw: {
            get_redraw(ctx, cmd);
            break;
        }

        case ws_cl_op_get_event: {
            get_event(ctx, cmd);
            break;
        }

        case ws_cl_op_create_screen_device:
            create_screen_device(ctx, cmd);
            break;

        case ws_cl_op_create_window_group:
            create_window_group(ctx, cmd);
            break;

        case ws_cl_op_create_window:
            create_window_base(ctx, cmd);
            break;

        case ws_cl_op_restore_default_hotkey:
            restore_hotkey(ctx, cmd);
            break;

        case ws_cl_op_create_gc:
            create_graphic_context(ctx, cmd);
            break;

        case ws_cl_op_create_sprite:
            create_sprite(ctx, cmd);
            break;

        case ws_cl_op_create_anim_dll:
            create_anim_dll(ctx, cmd);
            break;

        case ws_cl_op_create_click:
            create_click_dll(ctx, cmd);
            break;

        case ws_cl_op_create_graphics:
            create_graphic(ctx, cmd);
            break;

        case ws_cl_op_event_ready:
            get_ready(ctx, &cmd, event_listener_type_event);
            break;

        case ws_cl_op_redraw_ready:
            get_ready(ctx, &cmd, event_listener_type_redraw);
            break;

        case ws_cl_op_priority_key_ready:
            get_ready(ctx, &cmd, event_listener_type_priority_key);
            break;

        case ws_cl_op_get_focus_window_group: {
            get_focus_window_group(ctx, cmd);
            break;
        }

        case ws_cl_op_find_window_group_identifier: {
            find_window_group_id(ctx, cmd);
            break;
        }

        case ws_cl_op_find_window_group_identifier_thread: {
            find_window_group_id_thread(ctx, cmd);
            break;
        }

        case ws_cl_op_get_window_group_name_from_identifier: {
            get_window_group_name_from_id(ctx, cmd);
            break;
        }

        case ws_cl_op_window_group_list:
        case ws_cl_op_window_group_list_all_priorities:
        case ws_cl_op_window_group_list_and_chain:
        case ws_cl_op_window_group_list_and_chain_all_priorities: {
            get_window_group_list(ctx, cmd);
            break;
        }

        case ws_cl_op_clear_all_redraw_stores: {
            clear_all_redraw_stores(ctx, cmd);
            break;
        }

        case ws_cl_op_set_window_group_ordinal_position: {
            set_window_group_ordinal_position(ctx, cmd);
            break;
        }

        case ws_cl_op_get_def_mode_max_num_colors: {
            get_def_mode_max_num_colors(ctx, cmd);
            break;
        }

        case ws_cl_op_create_direct_screen_access: {
            create_dsa(ctx, cmd);
            break;
        }

        case ws_cl_op_get_color_mode_list: {
            get_color_mode_list(ctx, cmd);
            break;
        }

        case ws_cl_op_create_bitmap: {
            create_wsbmp(ctx, cmd);
            break;
        }

        case ws_cl_op_set_pointer_cursor_area:
            set_pointer_area(ctx, cmd);
            break;

        case ws_cl_op_set_pointer_cursor_position:
            set_pointer_cursor_position(ctx, cmd);
            break;

        case ws_cl_op_get_number_screen:
            get_number_of_screen(ctx, cmd);
            break;

        case ws_cl_op_get_focus_screen:
            get_focus_screen(ctx, cmd);
            break;

        case ws_cl_op_raw_event:
            add_raw_event(ctx, cmd);
            break;

        case ws_cl_op_set_keyboard_repeat_rate:
            set_keyboard_repeat_rate(ctx, cmd);
            break;

        case ws_cl_op_get_keyboard_repeat_rate:
            get_keyboard_repeat_rate(ctx, cmd);
            break;

        case ws_cl_op_redraw_ready_cancel:
            redraw_ready_cancel(ctx, cmd);
            break;

        case ws_cl_op_event_ready_cancel:
            event_ready_cancel(ctx, cmd);
            break;

        case ws_cl_op_get_modifier_state:
        case ws_cl_op_set_modifier_state:
            // No modifiers (Ctrl, Alt, ...) are considered yet.
            // Apps known to use this: Frogger (Lonely Cat Games)
            ctx.complete(epoc::error_none);
            break;

        default:
            LOG_INFO(SERVICE_WINDOW, "Unimplemented ClOp: 0x{:x}", cmd.header.op);
            break;
        }
    }

    ws::uid window_server_client::add_capture_key_notifier_to_server(epoc::event_capture_key_notifier &notifier) {
        const ws::uid id = ++get_ws().key_capture_uid_counter;
        notifier.id = id;

        window_server::key_capture_request_queue &rqueue = get_ws().key_capture_requests[notifier.keycode_];

        if (!rqueue.empty() && notifier.pri_ == 0) {
            notifier.pri_ = rqueue.top().pri_ + 1;
        }

        rqueue.push(std::move(notifier));

        return id;
    }

    void window_server_client::send_screen_change_events(epoc::screen *scr) {
        for (auto &request: screen_changes) {
            if (request.user->scr == scr) {
                epoc::event evt;
                evt.type = epoc::event_code::screen_change;

                get_ws().send_event_to_window_group(reinterpret_cast<epoc::window_group*>(request.user), evt);
            }
        }
    }

    void window_server_client::send_focus_group_change_events(epoc::screen *scr) {
        for (auto &request: focus_group_change_notifies) {
            if (request.user->scr == scr) {
                epoc::event evt;
                evt.type = epoc::event_code::focus_group_changed;
                evt.handle = request.user->client_handle;

                queue_event(evt);
            }
        }
    }
}

namespace eka2l1 {
    std::string get_winserv_name_by_epocver(const epocver ver) {
        if (ver < epocver::eka2) {
            return "Windowserver";
        }

        return "!Windowserver";
    }

    void window_server::load_wsini() {
        io_system *io = sys->get_io_system();
        std::optional<eka2l1::drive> drv;
        drive_number dn = drive_z;

        for (; dn >= drive_a; dn = (drive_number)((int)dn - 1)) {
            drv = io->get_drive_entry(dn);

            if (drv && drv->media_type == drive_media::rom) {
                break;
            }
        }

        std::u16string wsini_path;
        wsini_path += static_cast<char16_t>((char)dn + 'A');
        wsini_path += u":\\system\\data\\wsini.ini";

        auto wsini_path_host = io->get_raw_path(wsini_path);

        if (!wsini_path_host) {
            LOG_ERROR(SERVICE_WINDOW, "Can't find the window config file, app using window server will broken");
            return;
        }

        std::string wsini_path_host_utf8 = common::ucs2_to_utf8(*wsini_path_host);
        int err = ws_config.load(wsini_path_host_utf8.c_str());

        if (err != 0) {
            LOG_ERROR(SERVICE_WINDOW, "Loading wsini file broke with code {}", err);
        }
    }

    void window_server::parse_wsini() {
        common::ini_node_ptr no_redraw_storing_node = ws_config.find("NOREDRAWSTORING");
        if (no_redraw_storing_node || (kern->get_epoc_version() <= epocver::epoc81b)) {
            config_flags |= CONFIG_FLAG_NO_REDRAW_STORING;
        }

        common::ini_node_ptr window_mode_node = ws_config.find("WINDOWMODE");
        epoc::display_mode scr_mode_global = epoc::display_mode::color16ma;

        if (window_mode_node) {
            common::ini_pair *window_mode_pair = window_mode_node->get_as<common::ini_pair>();

            std::vector<std::string> modes;
            modes.resize(1);

            window_mode_pair->get(modes);

            device_manager *mngr = sys->get_device_manager();
            device *current_dvc = mngr->get_current();

            bool use_in_ini = true;

            if (kern->get_epoc_version() <= epocver::epoc6) {
                loader::rom *rom_info = kern->get_rom_info();
                const epoc::display_mode conv_res = epoc::get_display_mode_from_bpp(rom_info->header.eka1_diff1.bits_per_pixel, true);

                if (scr_mode_global != epoc::display_mode::color_last) {
                    scr_mode_global = conv_res;
                    use_in_ini = false;
                }
            }

            if (use_in_ini) {
                scr_mode_global = epoc::string_to_display_mode(modes[0]);

                // It seems to be so!!! Since games still use metainfo hacks at the beginning of screen buffer
                if (kern->is_eka1() && (epoc::get_bpp_from_display_mode(scr_mode_global) > 16)) {
                    scr_mode_global = epoc::display_mode::color64k;
                }
            }
        }

        bool is_auto_clear = false;
        bool flicker_free = kern->is_eka1() ? false : true;
        bool blit_offscreen = kern->is_eka1() ? false : true;

        common::ini_node_ptr auto_clear_node = ws_config.find("AUTOCLEAR");
        if (auto_clear_node) {
            std::uint32_t auto_clear_value = 0;

            common::ini_pair *auto_clear_pair = auto_clear_node->get_as<common::ini_pair>();
            if (auto_clear_pair->get(&auto_clear_value, 1, 0) == 1) {
                is_auto_clear = (auto_clear_value != 0);
            }
        }

        if (ws_config.find("FLICKERFREEREDRAW")) {
            flicker_free = true;
        }
        
        if (ws_config.find("BLTOFFSCREENBITMAP")) {
            blit_offscreen = true;
        }

        common::ini_node *screen_node = nullptr;
        int total_screen = 0;

        bool one_screen_only = false;

        // Screen size is assumed to be 176x208 by default
        static const eka2l1::vec2 ASSUMED_SCREEN_SIZE = { 176, 208 };

        do {
            std::string screen_key = "SCREEN";
            screen_key += std::to_string(total_screen);
            auto screen_node_shared = ws_config.find(screen_key.c_str());

            if (!screen_node_shared || !common::ini_section::is_my_type(screen_node_shared)) {
                if (total_screen == 0) {
                    screen_node = reinterpret_cast<common::ini_node *>(&ws_config);
                    one_screen_only = true;
                } else {
                    break;
                }
            } else {
                screen_node = screen_node_shared.get();
            }

            total_screen++;
            epoc::config::screen scr;

            scr.screen_number = total_screen - 1;
            scr.disp_mode = scr_mode_global;
            scr.auto_clear = is_auto_clear;
            scr.flicker_free = flicker_free;
            scr.blt_offscreen = blit_offscreen;

            int total_mode = 0;
            bool one_mode_only = false;

            do {
                std::string current_mode_str = std::to_string(total_mode + 1);
                std::string screen_mode_width_key = "SCR_WIDTH";
                screen_mode_width_key += current_mode_str;

                common::ini_node_ptr mode_width = screen_node->get_as<common::ini_section>()->find(screen_mode_width_key.c_str());

                std::string screen_mode_height_key = "SCR_HEIGHT";
                screen_mode_height_key += current_mode_str;

                common::ini_node_ptr mode_height = screen_node->get_as<common::ini_section>()->find(screen_mode_height_key.c_str());

                total_mode++;

                epoc::config::screen_mode scr_mode;
                scr_mode.screen_number = total_screen - 1;
                scr_mode.mode_number = total_mode;

                if (mode_width) {
                    mode_width->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&scr_mode.size.x),
                        1, 0);
                } else {
                    if (total_mode > 1)
                        break;

                    scr_mode.size.x = ASSUMED_SCREEN_SIZE.x;
                    one_mode_only = true;
                }

                if (mode_height) {
                    mode_height->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&scr_mode.size.y),
                        1, 0);
                } else {
                    if (total_mode > 1)
                        break;

                    scr_mode.size.y = ASSUMED_SCREEN_SIZE.y;
                    one_mode_only = true;
                }

                current_mode_str = std::to_string(total_mode);

                std::string screen_mode_rot_key = "SCR_ROTATION";
                screen_mode_rot_key += current_mode_str;

                common::ini_node_ptr mode_rot = screen_node->get_as<common::ini_section>()->find(screen_mode_rot_key.c_str());

                if (mode_rot) {
                    mode_rot->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&scr_mode.rotation),
                        1, 0);
                } else {
                    scr_mode.rotation = 0;
                }

                std::string screen_style_s60_key = "S60_SCR_STYLE_NAME";
                screen_style_s60_key += current_mode_str;

                common::ini_node_ptr style_node = screen_node->get_as<common::ini_section>()->find(screen_style_s60_key.c_str());

                if (style_node) {
                    std::vector<std::string> styles;
                    styles.resize(1);

                    style_node->get_as<common::ini_pair>()->get(styles);
                    scr_mode.style = styles[0];
                } else {
                    scr_mode.style = "";
                }

                scr.modes.push_back(scr_mode);
            } while (!one_mode_only);

            int total_hardware_state = 0;

            do {
                std::string current_state_num_str = std::to_string(total_hardware_state);
                std::string screen_mode_normal_mode_num_key = "S60_HWSTATE_SCREENMODE";
                screen_mode_normal_mode_num_key += current_state_num_str;

                common::ini_node_ptr mode_normal_node = screen_node->get_as<common::ini_section>()->find(
                    screen_mode_normal_mode_num_key.c_str());

                std::string screen_mode_alternate_mode_num_key = "S60_HWSTATE_ALT_SCREENMODE";
                screen_mode_alternate_mode_num_key += current_state_num_str;

                common::ini_node_ptr mode_alternate_node = screen_node->get_as<common::ini_section>()->find(
                    screen_mode_alternate_mode_num_key.c_str());

                total_hardware_state++;

                epoc::config::hardware_state state_cfg;
                state_cfg.state_number = total_hardware_state - 1;

                if (mode_normal_node || mode_alternate_node) {
                    mode_normal_node->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&state_cfg.mode_normal), 1, 0);

                    mode_alternate_node->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&state_cfg.mode_alternative), 1, 0);

                    // TODO: Switch key
                    scr.hardware_states.push_back(state_cfg);
                } else {
                    break;
                }
            } while (true);

            screen_configs.push_back(scr);
        } while ((screen_node != nullptr) && (!one_screen_only));
    }

    // TODO: Anim scheduler currently has no way to resize number of screens after construction.
    window_server::window_server(system *sys)
        : service::server(sys->get_kernel_system(), sys, nullptr, get_winserv_name_by_epocver(sys->get_symbian_version_use()), true, true)
        , bmp_cache(sys->get_kernel_system())
        , anim_sched(sys->get_kernel_system(), sys->get_ntimer(), 1)
        , screens(nullptr)
        , focus_screen_(nullptr)
        , key_shipper(this)
        , ws_global_mem_chunk(nullptr)
        , ws_code_chunk(nullptr)
        , ws_global_mem_allocator(nullptr)
        , sync_thread_code_offset(0)
        , initial_repeat_delay_(0)
        , next_repeat_delay_(0)
        , repeatable_event_(0)
        , config_flags(0) {
        REGISTER_IPC(window_server, init, ws_mess_init, "Ws::Init");
        REGISTER_IPC(window_server, send_to_command_buffer, ws_mess_command_buffer, "Ws::CommandBuffer");
        REGISTER_IPC(window_server, send_to_command_buffer, ws_mess_sync_msg_buf, "Ws::MessSyncBuf");

        // For addition mappings before actual game launches
        init_key_mappings();
    }

    window_server::~window_server() {
        if (!clients.empty()) {
            LOG_WARN(SERVICE_WINDOW, "Kernel is having a leakage with window server!");
            clients.clear();
        }

        drivers::graphics_driver *drv = get_graphics_driver();

        // Destroy all screens
        while (screens != nullptr) {
            epoc::screen *next = screens->next;
            screens->deinit(drv);
            delete screens;
            screens = next;
        }

        get_ntimer()->remove_event(repeatable_event_);
        bmp_cache.clean(drv);
    }

    drivers::graphics_driver *window_server::get_graphics_driver() {
        return get_system()->get_graphics_driver();
    }

    ntimer *window_server::get_ntimer() {
        return get_system()->get_ntimer();
    }

    kernel_system *window_server::get_kernel_system() {
        return get_system()->get_kernel_system();
    }

    constexpr std::int64_t input_update_us = 100;

    bool make_key_event(epoc::key_map &map, drivers::input_event &driver_evt_, epoc::event &guest_evt_) {
        // For up and down events, the keycode will always be 0
        // We still have to fill valid value for event_code::key
        guest_evt_.key_evt_.code = 0;
        guest_evt_.type = (driver_evt_.key_.state_ == drivers::key_state::released) ? epoc::event_code::key_up
                                                                                    : epoc::event_code::key_down;

        std::optional<std::uint32_t> key_received = std::nullopt;
        if (driver_evt_.type_ == drivers::input_event_type::key_raw)
            key_received = driver_evt_.key_.code_;
        else
            key_received = epoc::map_key_to_inputcode(map, driver_evt_.key_.code_);

        bool found_correspond_mapping = true;

        if (!key_received.has_value()) {
            key_received = driver_evt_.key_.code_;
            found_correspond_mapping = false;
        }

        guest_evt_.key_evt_.scancode = key_received.value();
        guest_evt_.key_evt_.repeats = 0; // TODO?
        guest_evt_.key_evt_.modifiers = 0;

        return found_correspond_mapping;
    }

    bool make_button_event(epoc::button_map &map, drivers::input_event &driver_evt_, epoc::event &guest_evt_) {
        guest_evt_.key_evt_.code = 0;
        guest_evt_.type = (driver_evt_.button_.state_ == drivers::button_state::pressed) ? epoc::event_code::key_down : epoc::event_code::key_up;

        std::optional<std::uint32_t> mapped = epoc::map_button_to_inputcode(map, driver_evt_.button_.controller_, driver_evt_.button_.button_);

        if (!mapped.has_value()) {
            return false;
        }

        guest_evt_.key_evt_.scancode = mapped.value();
        guest_evt_.key_evt_.repeats = 0; // TODO?
        guest_evt_.key_evt_.modifiers = 0;

        return true;
    }

    /**
     * make a guest pointer event from host mouse event, return true if success.
     */
    void window_server::make_mouse_event(drivers::input_event &driver_evt_, epoc::event &guest_evt_, epoc::screen *scr) {
        guest_evt_.type = epoc::event_code::touch;
        guest_evt_.adv_pointer_evt_.pos_z = driver_evt_.mouse_.pos_z_;
        guest_evt_.adv_pointer_evt_.ptr_num = driver_evt_.mouse_.mouse_id;
        guest_evt_.adv_pointer_evt_.modifier = epoc::event_modifier_adv_pointer;

        switch (driver_evt_.mouse_.button_) {
        case drivers::mouse_button_left: {
            if (driver_evt_.mouse_.action_ == drivers::mouse_action_repeat) {
                guest_evt_.adv_pointer_evt_.evtype = epoc::event_type::drag;
            } else {
                guest_evt_.adv_pointer_evt_.evtype = (driver_evt_.mouse_.action_ == drivers::mouse_action_press) ? epoc::event_type::button1down : epoc::event_type::button1up;
            }

            break;
        }
        case drivers::mouse_button_middle: {
            guest_evt_.adv_pointer_evt_.evtype = driver_evt_.mouse_.action_ == drivers::mouse_action_press ? epoc::event_type::button2down : epoc::event_type::button2up;
            break;
        }
        case drivers::mouse_button_right: {
            guest_evt_.adv_pointer_evt_.evtype = driver_evt_.mouse_.action_ == drivers::mouse_action_press ? epoc::event_type::button3down : epoc::event_type::button3up;
            break;
        }
        }

        scr->screen_mutex.lock();

        guest_evt_.adv_pointer_evt_.pos.x = static_cast<int>(static_cast<float>(driver_evt_.mouse_.pos_x_ - scr->absolute_pos.x)
            / scr->logic_scale_factor_x);
        guest_evt_.adv_pointer_evt_.pos.y = static_cast<int>(static_cast<float>(driver_evt_.mouse_.pos_y_ - scr->absolute_pos.y)
            / scr->logic_scale_factor_y);

        const int orgx = guest_evt_.adv_pointer_evt_.pos.x;
        const int orgy = guest_evt_.adv_pointer_evt_.pos.y;

        switch (scr->ui_rotation) {
        case 90:
            guest_evt_.adv_pointer_evt_.pos.y = scr->current_mode().size.y - orgx - 1;
            guest_evt_.adv_pointer_evt_.pos.x = orgy;

            break;

        case 180:
            guest_evt_.adv_pointer_evt_.pos.y = scr->current_mode().size.y - orgy - 1;
            guest_evt_.adv_pointer_evt_.pos.x = scr->current_mode().size.x - orgx - 1;

            break;

        case 270:
            guest_evt_.adv_pointer_evt_.pos.y = orgx;
            guest_evt_.adv_pointer_evt_.pos.x = scr->current_mode().size.x - orgy - 1;

            break;

        default:
            break;
        }

        // use on debugging
        // LOG_TRACE(SERVICE_WINDOW, "touch position ({}, {}), type {}", guest_evt_.adv_pointer_evt_.pos.x, guest_evt_.adv_pointer_evt_.pos.y, guest_evt_.adv_pointer_evt_.evtype);
        scr->screen_mutex.unlock();
    }

    void window_server::queue_input_from_driver(drivers::input_event &evt) {
        if (!loaded) {
            return;
        }

        evt.time_ = kern->universal_time();

        handle_input_from_driver(evt);
    }

    void window_server::handle_input_from_driver(drivers::input_event input_event) {
        epoc::event guest_event;

        epoc::window *root_current = get_current_focus_screen()->root->child;
        guest_event.time = kern->universal_time();

        if (!root_current) {
            return;
        }

        bool shipped = false;

        // Translate host event to guest event
        switch (input_event.type_) {
        case drivers::input_event_type::key:
        case drivers::input_event_type::key_raw:
            if (make_key_event(input_mapping.key_input_map, input_event, guest_event)) {
                key_shipper.add_new_event(guest_event);
                key_shipper.start_shipping();

                shipped = true;
            }

            break;

        case drivers::input_event_type::button:
            if (make_button_event(input_mapping.button_input_map, input_event, guest_event)) {
                key_shipper.add_new_event(guest_event);
                key_shipper.start_shipping();

                shipped = true;
            }

            break;

        case drivers::input_event_type::touch: {
            epoc::screen *scr = get_current_focus_screen();
            eka2l1::vec2 screen_size_scaled(static_cast<int>(std::roundf(scr->current_mode().size.x * scr->logic_scale_factor_x)),
                static_cast<int>(std::roundf(scr->current_mode().size.y * scr->logic_scale_factor_y)));

            if (scr->ui_rotation % 180 != 0) {
                std::swap(screen_size_scaled.x, screen_size_scaled.y);
            }

            eka2l1::rect screen_rect(scr->absolute_pos, screen_size_scaled);

            // For touch, try to map to keycode first...
            // If no correspond mapping is found as key, just treat it as touch
            if (screen_rect.contains(eka2l1::point(input_event.mouse_.pos_x_, input_event.mouse_.pos_y_))) {
                drivers::input_event original_input_evt = input_event;

                input_event.key_.code_ = epoc::KEYBIND_TYPE_MOUSE_CODE_BASE + input_event.mouse_.button_;

                switch (input_event.mouse_.action_) {
                case drivers::mouse_action_press:
                    input_event.key_.state_ = drivers::key_state::pressed;
                    break;

                case drivers::mouse_action_repeat:
                    input_event.key_.state_ = drivers::key_state::repeat;
                    break;

                case drivers::mouse_action_release:
                    input_event.key_.state_ = drivers::key_state::released;
                    break;

                default:
                    input_event.key_.state_ = drivers::key_state::pressed;
                    break;
                }

                if (!make_key_event(input_mapping.key_input_map, input_event, guest_event)) {
                    make_mouse_event(original_input_evt, guest_event, get_current_focus_screen());

                    touch_shipper.add_new_event(guest_event);
                    root_current->walk_tree(&touch_shipper, epoc::window_tree_walk_style::bonjour_children_and_previous_siblings);
                    touch_shipper.clear();
                } else {
                    if (input_event.key_.state_ != drivers::key_state::repeat) {
                        key_shipper.add_new_event(guest_event);
                        key_shipper.start_shipping();
                    }
                }

                shipped = true;
            }

            break;
        }

        default:
            LOG_ERROR(SERVICE_WINDOW, "Unknown driver event type {}", static_cast<int>(input_event.type_));
            break;
        }

        if (shipped) {
            kern->reset_inactivity_time();
        }
    }

    static void create_screen_buffer_for_dsa(kernel_system *kern, epoc::screen *scr) {
        // Try to create memory chunk at kernel mapping, for DSA
        const std::string chunk_name = fmt::format("ScreenBuffer{}", scr->number);

        // Add more size to prevent crashing.
        const std::size_t max_chunk_size = (scr->size().x + 100) * scr->size().y * 4;

        // Create chunk with maximum size (32-bit)
        kernel::chunk *buffer = kern->create<kernel::chunk>(kern->get_memory_system(), nullptr, chunk_name, 0,
            static_cast<address>(max_chunk_size), max_chunk_size, prot_read_write,
            kernel::chunk_type::normal, kernel::chunk_access::kernel_mapping,
            kernel::chunk_attrib::none);

        // Fill with white
        std::uint8_t *fill_start = reinterpret_cast<std::uint8_t *>(buffer->host_base());
        std::fill(fill_start, fill_start + max_chunk_size, 255);

        scr->screen_buffer_chunk = buffer;
    }

    void window_server::init_screens() {
        kernel_system *kern = get_kernel_system();

        // Create first screen
        screens = new epoc::screen(0, get_screen_config(0));
        epoc::screen *crr = screens;
        create_screen_buffer_for_dsa(kern, crr);

        // Create other available screens. Plugged in screen later will be created explicitly
        for (std::size_t i = 0; i < screen_configs.size() - 1; i++) {
            crr->next = new epoc::screen(1, get_screen_config(1));

            crr = crr->next;

            if ((crr->size().x != -1) && (crr->size().y != -1)) {
                create_screen_buffer_for_dsa(kern, crr);
            }
        }

        // Set default focus screen to be the first
        focus_screen_ = screens;

        config::state *config = kern->get_config();

        if (config) {
            set_screen_sync_buffer_option(config->screen_buffer_sync);
        }
    }

    epoc::screen *window_server::get_screen(const int number) {
        if (!screens) {
            do_base_init();
        }

        if (number == -1) {
            // Use default screen
            return screens;
        }

        epoc::screen *crr = screens;

        while (crr && crr->number != number) {
            crr = crr->next;
        }

        return crr;
    }

    epoc::screen *window_server::get_screens() {
        if (!loaded) {
            do_base_init();
        }

        return screens;
    }

    epoc::window_group *window_server::get_group_from_id(const epoc::ws::uid id) {
        epoc::screen *current = screens;

        while (current) {
            epoc::window_group *group = reinterpret_cast<epoc::window_group *>(current->root->child);
            while (group && (group->id != id)) {
                group = reinterpret_cast<epoc::window_group *>(group->sibling);
            }

            if (group && (group->id == id)) {
                return group;
            }

            current = current->next;
        }

        return nullptr;
    }

    epoc::window_group *window_server::get_starting_group() {
        epoc::screen *current = screens;

        while (current) {
            epoc::window_group *group = reinterpret_cast<epoc::window_group *>(current->root->child);
            if (group) {
                return group;
            }

            current = current->next;
        }

        return nullptr;
    }

    void window_server::emit_ws_thread_code() {
        // Emit stub code
        common::cpu_info info;
        info.bARMv7 = false;
        info.bASIMD = false;

        common::armgen::armx_emitter emitter(reinterpret_cast<std::uint8_t *>(ws_code_chunk->host_base()), info);
        sync_thread_code_offset = 0;

        // =========== DSA synchronization code ===============
        // TODO: Make a proper code handling DSA synchronization. For now infinite loop busy waiting.
        common::armgen::fixup_branch sync_loop_begin;

        emitter.PUSH(2, common::armgen::R4, common::armgen::R_LR);
        emitter.MOV(common::armgen::R4, common::armgen::R0);

        std::uint8_t *loop_begin = emitter.get_writeable_code_ptr();

        // Load euser to get WaitForRequest
        codeseg_ptr seg = kern->get_lib_manager()->load(u"euser.dll");
        if (seg) {
            address wait_for_request_addr = seg->lookup(nullptr, kern->is_eka1() ? 1210 : 604);

            emitter.MOV(common::armgen::R0, common::armgen::R4);
            emitter.MOVI2R(common::armgen::R12, wait_for_request_addr);
            emitter.BL(common::armgen::R12);
        }

        sync_loop_begin = emitter.B();

        emitter.flush_lit_pool();

        emitter.set_code_pointer(loop_begin);
        emitter.set_jump_target(sync_loop_begin);
    }

    void window_server::init_ws_mem() {
        if (!ws_global_mem_chunk) {
            ws_global_mem_chunk = kern->create_and_add<kernel::chunk>(
                                          kernel::owner_type::kernel,
                                          kern->get_memory_system(),
                                          nullptr,
                                          "WsGlobalMemChunk",
                                          0,
                                          0x2000,
                                          0x2000,
                                          prot_read_write,
                                          kernel::chunk_type::normal,
                                          kernel::chunk_access::global,
                                          kernel::chunk_attrib::none)
                                      .second;

            ws_code_chunk = kern->create_and_add<kernel::chunk>(
                                    kernel::owner_type::kernel,
                                    kern->get_memory_system(),
                                    nullptr,
                                    "WsCodeChunk",
                                    0,
                                    0x2000,
                                    0x2000,
                                    prot_read_write_exec,
                                    kernel::chunk_type::normal,
                                    kernel::chunk_access::global,
                                    kernel::chunk_attrib::none)
                                .second;

            ws_global_mem_allocator = std::make_unique<epoc::chunk_allocator>(ws_global_mem_chunk);
            emit_ws_thread_code();
        }
    }

    void window_server::delete_key_mapping(const std::uint32_t target) {
        for (auto ite = input_mapping.key_input_map.begin(); ite != input_mapping.key_input_map.end(); ite++) {
            if (ite->second == target) {
                input_mapping.key_input_map.erase(ite);
                return;
            }
        }

        for (auto ite = input_mapping.button_input_map.begin(); ite != input_mapping.button_input_map.end(); ite++) {
            if (ite->second == target) {
                input_mapping.button_input_map.erase(ite);
                return;
            }
        }
    }

    void window_server::init_key_mappings() {
        input_mapping.key_input_map.clear();
        input_mapping.button_input_map.clear();

        config::state *conf = kern->get_config();

        for (auto &kb : conf->keybinds.keybinds) {
            delete_key_mapping(kb.target);

            bool is_mouse = (kb.source.type == config::KEYBIND_TYPE_MOUSE);

            if ((kb.source.type == config::KEYBIND_TYPE_KEY) || is_mouse) {
                const std::uint32_t bind_keycode = (is_mouse ? epoc::KEYBIND_TYPE_MOUSE_CODE_BASE : 0) + kb.source.data.keycode;
                input_mapping.key_input_map[bind_keycode] = static_cast<epoc::std_scan_code>(kb.target);
            } else if (kb.source.type == config::KEYBIND_TYPE_CONTROLLER) {
                input_mapping.button_input_map[std::make_pair(kb.source.data.button.controller_id, kb.source.data.button.button_id)] = static_cast<epoc::std_scan_code>(kb.target);
            }
        }
    }

    void window_server::do_base_init() {
        load_wsini();
        parse_wsini();
        init_screens();
        init_ws_mem();
        init_repeatable();

        

        loaded = true;
    }

    void window_server::init_repeatable() {
        initial_repeat_delay_ = epoc::WS_DEFAULT_KEYBOARD_REPEAT_INIT_DELAY;
        next_repeat_delay_ = epoc::WS_DEFAULT_KEYBOARD_REPEAT_NEXT_DELAY;

        repeatable_event_ = kern->get_ntimer()->register_event("WsRepeatableKeyEvent",
            [this](std::uint64_t data, std::uint64_t microsecs_late) {
                const std::uint32_t scancode = static_cast<std::uint32_t>(data);
                const std::uint32_t code = static_cast<std::uint32_t>(data >> 32);

                epoc::event repeatable_evt;
                ntimer *timing = kern->get_ntimer();

                kern->lock();

                if (get_focus()) {
                    repeatable_evt.type = epoc::event_code::key;

                    repeatable_evt.handle = get_focus()->client_handle;
                    repeatable_evt.time = kern->universal_time();
                    repeatable_evt.key_evt_.code = code;
                    repeatable_evt.key_evt_.scancode = scancode;
                    repeatable_evt.key_evt_.repeats = 1;

                    // TODO: Mark the modifiers currently being held in the server,
                    // and add them to the flags here!
                    repeatable_evt.key_evt_.modifiers = epoc::event_modifier_repeatable;

                    kern->reset_inactivity_time();

                    // TODO: When we queued this the event up event may already been queued.
                    // We have to make sure it's queued before event up, if there's something
                    // wrong popping up...
                    get_focus()->queue_event(repeatable_evt);
                }

                if (cancel_repeatable_list.find(data) != cancel_repeatable_list.end()) {
                    cancel_repeatable_list.erase(data);
                } else {
                    // Schedule next repeat
                    timing->schedule_event(next_repeat_delay_, repeatable_event_, data);
                }

                kern->unlock();
            });
    }

    void window_server::connect(service::ipc_context &ctx) {
        std::optional<epoc::version> the_ver = service::get_server_version(kern, &ctx);

        if (!the_ver) {
            the_ver = std::make_optional<epoc::version>();
            the_ver->u32 = 0;
        }

        clients.emplace(ctx.msg->msg_session->unique_id(),
            std::make_unique<epoc::window_server_client>(ctx.msg->msg_session, ctx.msg->own_thr, the_ver.value()));

        server::connect(ctx);
    }

    void window_server::disconnect(service::ipc_context &ctx) {
        clients.erase(ctx.msg->msg_session->unique_id());
        server::disconnect(ctx);
    }

    epoc::window_server_client *window_server::get_client(const std::uint64_t unique_id) {
        auto ite = clients.find(unique_id);
        if (ite == clients.end()) {
            return nullptr;
        }

        return ite->second.get();
    }

    void window_server::init(service::ipc_context &ctx) {
        if (!loaded) {
            do_base_init();
        }

        ctx.complete(static_cast<std::uint32_t>(ctx.msg->msg_session->unique_id()));
    }

    epoc::config::screen *window_server::get_current_focus_screen_config() {
        int num = 0;
        if (focus_screen_) {
            return &focus_screen_->scr_config;
        }

        if (!loaded) {
            do_base_init();
        }

        assert(false && "Unreachable code");
        return nullptr;
    }

    void window_server::send_to_command_buffer(service::ipc_context &ctx) {
        clients[ctx.msg->msg_session->unique_id()]->parse_command_buffer(ctx);
    }

    void window_server::on_unhandled_opcode(service::ipc_context &ctx) {
        if (ctx.msg->function & ws_mess_async_service) {
            switch (ctx.msg->function & ~ws_mess_async_service) {
            case ws_cl_op_redraw_ready:
                clients[ctx.msg->msg_session->unique_id()]->get_ready(ctx, nullptr, epoc::event_listener_type_redraw);
                break;

            // Notify when an event is ringing, means that whenever
            // is occured within an object that belongs to a client that
            // created by the same thread as the requester, that requester
            // will be notify
            case ws_cl_op_event_ready:
                clients[ctx.msg->msg_session->unique_id()]->get_ready(ctx, nullptr, epoc::event_listener_type_event);
                break;

            default: {
                LOG_TRACE(SERVICE_WINDOW, "Unhandle async code: {}", ctx.msg->function & ~ws_mess_async_service);
                break;
            }
            }
        } else {
            ctx.complete(epoc::error_none);
        }
    }

    struct window_group_tree_moonwalker : public epoc::window_tree_walker {
        epoc::screen *scr;
        std::uint32_t total;
        std::uint32_t max;
        std::uint8_t flags;
        std::int32_t accept_pri;
        void *buffer_vector;

        enum {
            FLAGS_GET_CHAIN = 1 << 0
        };

        // Unlike on Symbian, I just try to make window group recursive... Hopefully the stack opens their heart..
        explicit window_group_tree_moonwalker(epoc::screen *scr, void *buffer_vector, const std::uint8_t flags,
            const std::int32_t accept_pri, const std::uint32_t max)
            : scr(scr)
            , total(0)
            , max(max)
            , flags(flags)
            , accept_pri(accept_pri)
            , buffer_vector(buffer_vector) {
        }

        bool do_it(epoc::window *win) {
            if (win && (win->type != epoc::window_kind::group)) {
                return false;
            }

            if ((accept_pri != -1) && (win->priority != accept_pri)) {
                return false;
            }

            total++;

            if (buffer_vector) {
                if (flags & FLAGS_GET_CHAIN) {
                    epoc::window_group_chain_info *infos = reinterpret_cast<decltype(infos)>(buffer_vector);

                    epoc::window_group_chain_info chain_info;
                    chain_info.id = win->id;
                    chain_info.parent_id = 0;

                    if (win->parent && win->parent->type == epoc::window_kind::group) {
                        // Chain!
                        chain_info.parent_id = win->parent->id;
                    }

                    infos[total - 1] = chain_info;

                    if (total >= max) {
                        return true;
                    }
                } else {
                    std::uint32_t *infos = reinterpret_cast<decltype(infos)>(buffer_vector);
                    infos[total - 1] = win->id;

                    if (total >= max) {
                        return true;
                    }
                }
            }

            return false;
        }
    };

    std::uint32_t window_server::get_total_window_groups(const int pri, const int scr_num) {
        epoc::screen *scr = get_screen(scr_num);

        if (!scr) {
            LOG_TRACE(SERVICE_WINDOW, "Screen number {} doesnt exist", scr_num);
            return false;
        }

        bool hit_priority = false;

        // Tchhh... Im referencing a game...
        window_group_tree_moonwalker walker(scr, nullptr, 0, pri, -1);
        scr->root->child->walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children_and_previous_siblings);

        return walker.total;
    }

    std::uint32_t window_server::get_window_group_list(std::uint32_t *ids, const std::uint32_t max, const int pri, const int scr_num) {
        epoc::screen *scr = get_screen(scr_num);

        if (!scr) {
            LOG_TRACE(SERVICE_WINDOW, "Screen number {} doesnt exist", scr_num);
            return 0;
        }

        window_group_tree_moonwalker walker(scr, ids, 0, pri, max);
        scr->root->child->walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children_and_previous_siblings);

        return walker.total;
    }

    std::uint32_t window_server::get_window_group_list_and_chain(epoc::window_group_chain_info *infos, const std::uint32_t max, const int pri, const int scr_num) {
        epoc::screen *scr = get_screen(scr_num);

        if (!scr) {
            LOG_TRACE(SERVICE_WINDOW, "Screen number {} doesnt exist", scr_num);
            return false;
        }

        window_group_tree_moonwalker walker(scr, infos, window_group_tree_moonwalker::FLAGS_GET_CHAIN, pri, max);
        scr->root->child->walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children_and_previous_siblings);

        return walker.total;
    }

    fbs_server *window_server::get_fbs_server() {
        if (!fbss) {
            fbss = reinterpret_cast<fbs_server *>(&(*sys->get_kernel_system()->get_by_name<service::server>(
                epoc::get_fbs_server_name_by_epocver(sys->get_symbian_version_use()))));
        }

        return fbss;
    }

    epoc::chunk_allocator *window_server::allocator() {
        if (!ws_global_mem_allocator) {
            init_ws_mem();
        }

        return ws_global_mem_allocator.get();
    }

    address window_server::sync_thread_code_address() {
        if (!ws_global_mem_allocator) {
            init_ws_mem();
        }

        return ws_code_chunk->base(nullptr).ptr_address() + sync_thread_code_offset;
    }

    epoc::bitwise_bitmap *window_server::get_bitmap(const std::uint32_t h) {
        fbsbitmap *bmp = get_fbs_server()->get<fbsbitmap>(h);
        if (bmp) {
            bmp = bmp->final_clean();
            return bmp->bitmap_;
        }

        return nullptr;
    }

    fbsbitmap *window_server::get_raw_fbsbitmap(const std::uint32_t h) {
        return get_fbs_server()->get<fbsbitmap>(h);
    }

    void window_server::set_keyboard_repeat_rate(const std::uint64_t initial_time, const std::uint64_t next_time) {
        // TODO: Use these parameters.
        initial_repeat_delay_ = initial_time;
        next_repeat_delay_ = next_time;
    }

    void window_server::get_keyboard_repeat_rate(std::uint64_t &initial_time, std::uint64_t &next_time) {
        initial_time = initial_repeat_delay_;
        next_time = next_repeat_delay_;
    }

    void window_server::send_event_to_window_group(epoc::window_group *group, const epoc::event &evt) {
        epoc::window_server_client *cli = group->client;

        epoc::event evt_copy = evt;
        evt_copy.handle = group->client_handle;

        cli->queue_event(evt_copy);
    }

    void window_server::send_event_to_window_groups(const epoc::event &evt) {
        epoc::screen *scr = get_screens();

        while (scr) {
            epoc::window_group *group = scr->get_group_chain();
            while (group) {
                send_event_to_window_group(group, evt);
                group = reinterpret_cast<epoc::window_group *>(group->sibling);
            }

            scr = scr->next;
        }
    }

    void window_server::send_screen_change_events(epoc::screen *scr) {
        for (auto &[uid, cli]: clients) {
            if (cli) {
                cli->send_screen_change_events(scr);
            }
        }
    }

    void window_server::send_focus_group_change_events(epoc::screen *scr) {
        for (auto &[uid, cli]: clients) {
            if (cli) {
                cli->send_focus_group_change_events(scr);
            }
        }
    }

    void window_server::set_screen_sync_buffer_option(const int option) {
        bool on = false;

        if (option == config::screen_buffer_sync_option_preferred) {
            if (kern->is_eka1()) {
                on = true;
            } else {
                on = false;
            }
        } else if (option == config::screen_buffer_sync_option_on) {
            on = true;
        }

        for (epoc::screen *scr = screens; scr; scr = scr->next) {
            scr->sync_screen_buffer = on;
        }
    }
}
