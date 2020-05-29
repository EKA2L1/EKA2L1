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
#include <services/window/classes/bitmap.h>
#include <services/window/classes/dsa.h>
#include <services/window/classes/gctx.h>
#include <services/window/classes/plugins/animdll.h>
#include <services/window/classes/plugins/clickdll.h>
#include <services/window/classes/plugins/sprite.h>
#include <services/window/classes/scrdvc.h>
#include <services/window/classes/winbase.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/classes/wsobj.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/rgb.h>

#include <utils/err.h>

#include <epoc/epoc.h>
#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <vfs/vfs.h>

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

    void window_server_client::parse_command_buffer(service::ipc_context &ctx) {
        std::optional<std::string> dat = ctx.get_arg<std::string>(cmd_slot);

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
        , cli_version(ver)
        , primary_device(nullptr)
        , uid_counter(0) {
    }

    void window_server_client::execute_commands(service::ipc_context &ctx, std::vector<ws_cmd> cmds) {
        for (auto &cmd : cmds) {
            if (cmd.obj_handle == guest_session->unique_id()) {
                execute_command(ctx, cmd);
            } else {
                if (auto obj = get_object(cmd.obj_handle)) {
                    obj->execute_command(ctx, cmd);
                }
            }
        }
    }

    std::uint32_t window_server_client::queue_redraw(epoc::window_user *user) {
        // Calculate the priority
        const std::uint32_t id = redraws.queue_event(epoc::redraw_event{ user->get_client_handle(), user->irect.top,
                                                         vec2(user->irect.top.x + user->irect.size.x,
                                                             user->irect.top.y + user->irect.size.y) },
            user->redraw_priority());

        user->irect = eka2l1::rect({ 0, 0 }, { 0, 0 });
        return id;
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
            LOG_WARN("Object handle is invalid {}", handle);
            return nullptr;
        }

        return objects[idx - 1].get();
    }

    bool window_server_client::delete_object(const std::uint32_t handle) {
        const std::uint32_t idx = handle & 0xFFFF;

        if (idx > objects.size() || idx == 0) {
            LOG_WARN("Object handle is invalid {}", handle);
            return false;
        }

        objects[idx - 1].reset();
        return true;
    }

    void window_server_client::create_screen_device(service::ipc_context &ctx, ws_cmd &cmd) {
        LOG_INFO("Create screen device.");

        ws_cmd_screen_device_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);

        // Get screen object
        epoc::screen *target_screen = get_ws().get_screen(header->num_screen);

        if (!target_screen) {
            LOG_ERROR("Can't find screen object with number {}", header->num_screen);
            ctx.set_request_status(epoc::error_not_found);
            return;
        }

        window_client_obj_ptr device = std::make_unique<epoc::screen_device>(this, target_screen);

        if (!primary_device) {
            primary_device = reinterpret_cast<epoc::screen_device *>(device.get());
        }

        ctx.set_request_status(add_object(device));
    }

    void window_server_client::create_dsa(service::ipc_context &ctx, ws_cmd &cmd) {
        window_client_obj_ptr dsa_inst = std::make_unique<epoc::dsa>(this);
        if (!dsa_inst) {
            ctx.set_request_status(epoc::error_general);
            return;
        }

        ctx.set_request_status(add_object(dsa_inst));
    }

    void window_server_client::restore_hotkey(service::ipc_context &ctx, ws_cmd &cmd) {
        THotKey key = *reinterpret_cast<THotKey *>(cmd.data_ptr);

        LOG_WARN("Unknown restore key op.");
    }

    void window_server_client::create_window_group(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_window_group_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);
        int device_handle = header->screen_device_handle;

        epoc::screen_device *device_ptr;

        if (device_handle <= 0) {
            device_ptr = primary_device;
        } else {
            device_ptr = reinterpret_cast<epoc::screen_device *>(get_object(device_handle));
        }

        if (!device_ptr) {
            device_ptr = primary_device;
        }

        epoc::window *parent_group = reinterpret_cast<epoc::window *>(get_object(header->parent_id));

        if (!parent_group) {
            LOG_WARN("Unable to find parent for new group with ID = 0x{:x}. Use root", header->parent_id);
            parent_group = device_ptr->scr->root.get();
        }

        window_client_obj_ptr group = std::make_unique<epoc::window_group>(this, device_ptr->scr, parent_group, header->client_handle);
        epoc::window_group *group_casted = reinterpret_cast<epoc::window_group *>(group.get());

        if (header->focus) {
            group_casted->set_receive_focus(true);
            device_ptr->scr->update_focus(&get_ws(), nullptr);
        }

        // Give it a nice name.
        // We can give it name with id, but too much hassle
        group_casted->name = common::utf8_to_ucs2(fmt::format("WindowGroup{:X}", header->client_handle));

        std::uint32_t id = add_object(group);
        ctx.set_request_status(id);
    }

    void window_server_client::create_window_base(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_window_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);
        epoc::window *parent = reinterpret_cast<epoc::window *>(get_object(header->parent));

        if (!parent) {
            LOG_WARN("Unable to find parent for new window with ID = 0x{:x}. Use root", header->parent);
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        if (parent->type != window_kind::group && parent->type != window_kind::client) {
            LOG_ERROR("The parent of window user must be a group or another user!");
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        // We have to be child's parent child, which is top user.
        window_client_obj_ptr win = std::make_unique<epoc::window_user>(this, parent->scr,
            (parent->type == window_kind::group) ? parent->child : parent, header->win_type,
            header->dmode, header->client_handle);

        ctx.set_request_status(add_object(win));
    }

    void window_server_client::create_graphic_context(service::ipc_context &ctx, ws_cmd &cmd) {
        window_client_obj_ptr gcontext = std::make_unique<epoc::graphic_context>(this);
        ctx.set_request_status(add_object(gcontext));
    }

    void window_server_client::create_sprite(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_create_sprite_header *sprite_header = reinterpret_cast<decltype(sprite_header)>(cmd.data_ptr);
        epoc::window *win = reinterpret_cast<epoc::window *>(get_object(sprite_header->window_handle));

        if (!win) {
            LOG_WARN("Window handle is invalid! Abort");
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        window_client_obj_ptr spr = std::make_unique<epoc::sprite>(this, win->scr, win, sprite_header->base_pos);
        ctx.set_request_status(add_object(spr));
    }

    void window_server_client::create_anim_dll(service::ipc_context &ctx, ws_cmd &cmd) {
        const int dll_name_length = *reinterpret_cast<int *>(cmd.data_ptr);
        const char16_t *dll_name_ptr = reinterpret_cast<char16_t *>(reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + sizeof(int));

        const std::u16string dll_name(dll_name_ptr, dll_name_length);

        LOG_TRACE("Create ANIMDLL for {}, stubbed object", common::ucs2_to_utf8(dll_name));

        window_client_obj_ptr animdll = std::make_unique<epoc::anim_dll>(this, nullptr);
        ctx.set_request_status(add_object(animdll));
    }

    void window_server_client::create_click_dll(service::ipc_context &ctx, ws_cmd &cmd) {
        LOG_TRACE("Create CLICKDLL (button click sound plugin), stubbed object");

        window_client_obj_ptr clickdll = std::make_unique<epoc::click_dll>(this, nullptr);
        ctx.set_request_status(add_object(clickdll));
    }

    void window_server_client::create_wsbmp(service::ipc_context &ctx, ws_cmd &cmd) {
        const std::uint32_t bmp_handle = *reinterpret_cast<const std::uint32_t *>(cmd.data_ptr);
        fbs_server *serv = get_ws().get_fbs_server();

        // Try to get the fbsbitmap
        fbsbitmap *bmp = serv->get<fbsbitmap>(bmp_handle);

        if (!bmp) {
            ctx.set_request_status(epoc::error_bad_handle);
            return;
        }

        window_client_obj_ptr wsbmpobj = std::make_unique<epoc::wsbitmap>(this, bmp);
        ctx.set_request_status(add_object(wsbmpobj));
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

        ctx.write_arg_pkg(reply_slot, reinterpret_cast<std::uint8_t *>(&ids[0]), static_cast<std::uint32_t>(ids.size()));
        ctx.set_request_status(static_cast<int>(total));
    }

    void window_server_client::get_number_of_window_groups(service::ipc_context &ctx, ws_cmd &cmd) {
        ctx.set_request_status(static_cast<int>(get_ws().get_total_window_groups(
            cmd.header.op == ws_cl_op_num_window_groups ? *static_cast<int *>(cmd.data_ptr) : -1)));
    }

    void window_server_client::send_event_to_window_group(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_send_event_to_window_group *evt = reinterpret_cast<decltype(evt)>(cmd.data_ptr);
        queue_event(evt->evt);

        ctx.set_request_status(epoc::error_none);
    }

    void window_server_client::find_window_group_id(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_find_window_group_identifier *find_info = reinterpret_cast<decltype(find_info)>(cmd.data_ptr);
        epoc::window_group *group = reinterpret_cast<epoc::window_group *>(primary_device->scr->root->child);

        if (find_info->previous_id) {
            // Find our lost sibling!!!!! Bring him to me....
            group = get_ws().get_group_from_id(find_info->previous_id);

            if (!group) {
                LOG_ERROR("Previous group sibling not found with id {}", find_info->previous_id);
                ctx.set_request_status(epoc::error_not_found);
                return;
            }

            // She's sweet but sibling...
            // A little sibling
            group = reinterpret_cast<epoc::window_group *>(group->sibling);
        }

        const char16_t *win_group_name_ptr = reinterpret_cast<char16_t *>(find_info + 1);
        const std::u16string win_group_name(win_group_name_ptr, find_info->length);

        for (; group; group = reinterpret_cast<epoc::window_group *>(group->sibling)) {
            if (common::compare_ignore_case(group->name.substr(find_info->offset), win_group_name) == 0) {
                ctx.set_request_status(group->id);
                return;
            }
        }

        ctx.set_request_status(epoc::error_not_found);
    }

    void window_server_client::set_pointer_cursor_mode(service::ipc_context &ctx, ws_cmd &cmd) {
        // TODO: Check errors
        if (get_ws().get_focus() && get_ws().get_focus()->client == this) {
            get_ws().cursor_mode() = *reinterpret_cast<epoc::pointer_cursor_mode *>(cmd.data_ptr);
            ctx.set_request_status(epoc::error_none);
            return;
        }

        ctx.set_request_status(epoc::error_permission_denied);
    }

    void window_server_client::get_window_group_client_thread_id(service::ipc_context &ctx, ws_cmd &cmd) {
        std::uint32_t group_id = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        epoc::window_group *win = get_ws().get_group_from_id(group_id);

        if (!win || win->type != window_kind::group) {
            LOG_TRACE("Can't find group with id {}", group_id);
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        const std::uint32_t thr_id = win->client->get_client()->unique_id();

        ctx.write_arg_pkg<std::uint32_t>(reply_slot, thr_id);
        ctx.set_request_status(epoc::error_none);
    }

    void window_server_client::get_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        auto evt = redraws.get_evt_opt();

        if (!evt) {
            ctx.set_request_status(epoc::error_not_found);
            return;
        }

        ctx.write_arg_pkg<epoc::redraw_event>(reply_slot, *evt);
        ctx.set_request_status(epoc::error_none);
    }

    void window_server_client::get_event(service::ipc_context &ctx, ws_cmd &cmd) {
        auto evt = events.get_event();

        // Allow the context to shrink if needed, since the struct certainly got larger as Symbian
        // grows. S^3 has advance pointer struct which along takes 56 bytes buffer.
        ctx.write_arg_pkg<epoc::event>(reply_slot, evt, nullptr, true);
        ctx.set_request_status(epoc::error_none);
    }

    void window_server_client::get_focus_window_group(service::ipc_context &ctx, ws_cmd &cmd) {
        // TODO: Epoc < 9
        if (cmd.header.cmd_len == 0) {
            ctx.set_request_status(primary_device->scr->focus->id);
            return;
        }

        int screen_num = *reinterpret_cast<int *>(cmd.data_ptr);
        epoc::screen *scr = get_ws().get_screen(screen_num);

        if (!scr) {
            LOG_ERROR("Invalid screen number {}", screen_num);
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        ctx.set_request_status(scr->focus->id);
    }

    void window_server_client::get_window_group_name_from_id(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_get_window_group_name_from_id *find_info = reinterpret_cast<decltype(find_info)>(cmd.data_ptr);
        epoc::window_group *group = get_ws().get_group_from_id(find_info->id);

        if (!group || group->type != window_kind::group) {
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        if (group->name.length() == 0) {
            ctx.set_request_status(epoc::error_not_ready);
            return;
        }

        std::uint32_t len_to_write = std::min<std::uint32_t>(static_cast<std::uint32_t>(find_info->max_len),
            static_cast<std::uint32_t>(group->name.length()));

        const std::u16string to_write = group->name.substr(0, len_to_write);

        ctx.write_arg(reply_slot, to_write);
        ctx.set_request_status(epoc::error_none);
    }

    struct window_clear_store_walker : public epoc::window_tree_walker {
        bool do_it(epoc::window *win) {
            if (win->type == window_kind::group) {
                win->client->trigger_redraw();
            }

            if (win->type == window_kind::client) {
                epoc::window_user *user = reinterpret_cast<epoc::window_user *>(win);
                user->clear_redraw_store();
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

        ctx.set_request_status(epoc::error_none);
    }

    void window_server_client::set_window_group_ordinal_position(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_set_window_group_ordinal_position *set = reinterpret_cast<decltype(set)>(cmd.data_ptr);
        epoc::window_group *group = get_ws().get_group_from_id(set->identifier);

        if (!group || group->type != window_kind::group) {
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        group->set_position(set->ord_pos);
        ctx.set_request_status(epoc::error_none);
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
            LOG_ERROR("Can't find screen object with number {}, using the default screen", screen_num);
            scr = get_ws().get_current_focus_screen();
        }

        def_mode_max_num_colors result;
        scr->get_max_num_colors(result.num_color, result.num_grey);
        result.disp_mode = scr->disp_mode;

        ctx.write_arg_pkg<def_mode_max_num_colors>(reply_slot, result);
        ctx.set_request_status(epoc::error_none);
    }

    void window_server_client::get_color_mode_list(service::ipc_context &ctx, ws_cmd &cmd) {
        std::int32_t screen_num = *reinterpret_cast<std::int32_t *>(cmd.data_ptr);
        epoc::screen *scr = get_ws().get_screen(screen_num);

        if (!scr) {
            LOG_ERROR("Can't find requested screen {}, use focus one", screen_num);
            scr = get_ws().get_current_focus_screen();
        }

        ctx.set_request_status(1 << (static_cast<std::uint8_t>(scr->disp_mode) - 1));
    }

    void window_server_client::set_pointer_area(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_cmd_set_pointer_cursor_area *area_info = reinterpret_cast<ws_cmd_set_pointer_cursor_area *>(cmd.data_ptr);
        area_info->pointer_area.transform_from_symbian_rectangle();

        // Set for the default screen
        epoc::screen *scr = get_ws().get_screen(0);
        assert(scr);

        scr->pointer_areas_[area_info->mode] = area_info->pointer_area;

        ctx.set_request_status(epoc::error_none);
    }

    void window_server_client::set_pointer_cursor_position(service::ipc_context &ctx, ws_cmd &cmd) {
        eka2l1::vec2 *pos = reinterpret_cast<eka2l1::vec2 *>(cmd.data_ptr);
        // Set for the default screen
        epoc::screen *scr = get_ws().get_screen(0);
        assert(scr);

        scr->pointer_cursor_pos_ = *pos;
        ctx.set_request_status(epoc::error_none);
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

        ctx.set_request_status(connected_count);
    }

    // This handle both sync and async
    void window_server_client::execute_command(service::ipc_context &ctx, ws_cmd cmd) {
        //LOG_TRACE("Window client op: {}", (int)cmd.header.op);

        switch (cmd.header.op) {
        // Gets the total number of window groups with specified priority currently running
        // in the window server.
        case ws_cl_op_num_window_groups:
        case ws_cl_op_num_groups_all_priorities: {
            get_number_of_window_groups(ctx, cmd);
            break;
        }

        case ws_cl_op_send_event_to_window_group: {
            send_event_to_window_group(ctx, cmd);
            break;
        }

        case ws_cl_op_compute_mode: {
            LOG_TRACE("Setting compute mode not supported, instead stubbed");
            ctx.set_request_status(epoc::error_none);

            break;
        }

        case ws_cl_op_set_pointer_cursor_mode: {
            set_pointer_cursor_mode(ctx, cmd);
            break;
        }

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

        case ws_cl_op_event_ready:
            break;

        case ws_cl_op_get_focus_window_group: {
            get_focus_window_group(ctx, cmd);
            break;
        }

        case ws_cl_op_find_window_group_identifier: {
            find_window_group_id(ctx, cmd);
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

        default:
            LOG_INFO("Unimplemented ClOp: 0x{:x}", cmd.header.op);
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
}

namespace eka2l1 {
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
            LOG_ERROR("Can't find the window config file, app using window server will broken");
            return;
        }

        std::string wsini_path_host_utf8 = common::ucs2_to_utf8(*wsini_path_host);
        int err = ws_config.load(wsini_path_host_utf8.c_str());

        if (err != 0) {
            LOG_ERROR("Loading wsini file broke with code {}", err);
        }
    }

    void window_server::parse_wsini() {
        common::ini_node_ptr window_mode_node = ws_config.find("WINDOWMODE");
        epoc::display_mode scr_mode_global = epoc::display_mode::color16ma;

        if (window_mode_node) {
            common::ini_pair *window_mode_pair = window_mode_node->get_as<common::ini_pair>();

            std::vector<std::string> modes;
            modes.resize(1);

            window_mode_pair->get(modes);

            scr_mode_global = epoc::string_to_display_mode(modes[0]);
        }

        common::ini_node *screen_node = nullptr;
        int total_screen = 0;

        bool one_screen_only = false;

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

            int total_mode = 0;

            do {
                std::string current_mode_str = std::to_string(total_mode + 1);
                std::string screen_mode_width_key = "SCR_WIDTH";
                screen_mode_width_key += current_mode_str;

                common::ini_node_ptr mode_width = screen_node->get_as<common::ini_section>()->find(screen_mode_width_key.c_str());

                if (!mode_width) {
                    break;
                }

                std::string screen_mode_height_key = "SCR_HEIGHT";
                screen_mode_height_key += current_mode_str;

                common::ini_node_ptr mode_height = screen_node->get_as<common::ini_section>()->find(screen_mode_height_key.c_str());

                total_mode++;

                epoc::config::screen_mode scr_mode;
                scr_mode.screen_number = total_screen - 1;
                scr_mode.mode_number = total_mode;

                mode_width->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&scr_mode.size.x),
                    1, 0);
                mode_height->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&scr_mode.size.y),
                    1, 0);

                current_mode_str = std::to_string(total_mode);

                std::string screen_mode_rot_key = "SCR_ROTATION";
                screen_mode_rot_key += current_mode_str;

                common::ini_node_ptr mode_rot = screen_node->get_as<common::ini_section>()->find(screen_mode_rot_key.c_str());

                mode_rot->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&scr_mode.rotation),
                    1, 0);

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
            } while (true);

            screen_configs.push_back(scr);
        } while ((screen_node != nullptr) && !one_screen_only);
    }

    // TODO: Anim scheduler currently has no way to resize number of screens after construction.
    window_server::window_server(system *sys)
        : service::server(sys, WINDOW_SERVER_NAME, true, true)
        , bmp_cache(sys->get_kernel_system())
        , anim_sched(sys->get_kernel_system(), sys->get_ntimer(), 1)
        , screens(nullptr)
        , focus_screen_(nullptr) {
        REGISTER_IPC(window_server, init, EWservMessInit,
            "Ws::Init");
        REGISTER_IPC(window_server, send_to_command_buffer, EWservMessCommandBuffer,
            "Ws::CommandBuffer");
        REGISTER_IPC(window_server, send_to_command_buffer, EWservMessSyncMsgBuf,
            "Ws::MessSyncBuf");
    }

    window_server::~window_server() {
        drivers::graphics_driver *drv = get_graphics_driver();

        // Destroy all screens
        while (screens != nullptr) {
            epoc::screen *next = screens->next;
            screens->deinit(drv);
            delete screens;
            screens = next;
        }
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

    constexpr std::int64_t input_update_us = 10;

    static void make_key_event(drivers::input_event &driver_evt_, epoc::event &guest_evt_) {
        // For up and down events, the keycode will always be 0
        // We still have to fill valid value for event_code::key
        guest_evt_.key_evt_.code = 0;
        guest_evt_.type = (driver_evt_.key_.state_ == drivers::key_state::pressed) ? epoc::event_code::key_down : epoc::event_code::key_up;
        guest_evt_.key_evt_.scancode = static_cast<std::uint32_t>(driver_evt_.key_.code_);
        guest_evt_.key_evt_.repeats = 0; // TODO?
        guest_evt_.key_evt_.modifiers = 0;
    }

    /**
     * make a guest pointer event from host mouse event, return true if success.
     */
    void window_server::make_mouse_event(drivers::input_event &driver_evt_, epoc::event &guest_evt_, epoc::screen *scr) {
        guest_evt_.type = epoc::event_code::touch;

        switch (driver_evt_.mouse_.button_) {
        case drivers::mouse_button::left: {
            if (driver_evt_.mouse_.action_ == drivers::mouse_action::repeat) {
                guest_evt_.adv_pointer_evt_.evtype = epoc::event_type::drag;
            } else {
                guest_evt_.adv_pointer_evt_.evtype = (driver_evt_.mouse_.action_ == drivers::mouse_action::press) ? epoc::event_type::button1down : epoc::event_type::button1up;
            }

            break;
        }
        case drivers::mouse_button::middle: {
            guest_evt_.adv_pointer_evt_.evtype = driver_evt_.mouse_.action_ == drivers::mouse_action::press ? epoc::event_type::button2down : epoc::event_type::button2up;
            break;
        }
        case drivers::mouse_button::right: {
            guest_evt_.adv_pointer_evt_.evtype = driver_evt_.mouse_.action_ == drivers::mouse_action::press ? epoc::event_type::button3down : epoc::event_type::button3up;
            break;
        }
        }

        scr->screen_mutex.lock();
        guest_evt_.adv_pointer_evt_.pos.x = static_cast<int>(static_cast<float>(driver_evt_.mouse_.pos_x_ - scr->absolute_pos.x)
            / scr->scale.x);
        guest_evt_.adv_pointer_evt_.pos.y = static_cast<int>(static_cast<float>(driver_evt_.mouse_.pos_y_ - scr->absolute_pos.y)
            / scr->scale.y);

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

        scr->screen_mutex.unlock();
    }

    void window_server::queue_input_from_driver(drivers::input_event &evt) {
        if (!loaded) {
            return;
        }

        const std::lock_guard<std::mutex> guard(input_queue_mut);
        input_events.push(std::move(evt));
    }

    void window_server::handle_inputs_from_driver(std::uint64_t userdata, int nn_late) {
        if (!focus_screen_ || !focus_screen_->focus) {
            sys->get_ntimer()->schedule_event(input_update_us - nn_late, input_handler_evt_, userdata);
            return;
        }

        epoc::event guest_event;

        drivers::input_event_type last_event_type = drivers::input_event_type::none;

        epoc::window_pointer_focus_walker touch_shipper;
        epoc::window_key_shipper key_shipper(this);

        epoc::window *root_current = get_current_focus_screen()->root->child;
        std::unique_lock<std::mutex> unq(input_queue_mut);

        auto flush_events = [&]() {
            unq.unlock();

            // Delivery events stored
            switch (last_event_type) {
            case drivers::input_event_type::touch:
                root_current->walk_tree(&touch_shipper, epoc::window_tree_walk_style::bonjour_children_and_previous_siblings);
                touch_shipper.clear();
                break;

            case drivers::input_event_type::key:
                key_shipper.start_shipping();
                break;

            default:
                LOG_ERROR("Unknown driver event type {}", static_cast<int>(last_event_type));
                break;
            }

            unq.lock();
        };

        drivers::input_event input_event;

        // Processing the events, translate them to cool things
        while (!input_events.empty()) {
            input_event = std::move(input_events.back());
            input_events.pop();

            // Translate host event to guest event
            switch (input_event.type_) {
            case drivers::input_event_type::key:
                make_key_event(input_event, guest_event);
                break;

            case drivers::input_event_type::touch:
                make_mouse_event(input_event, guest_event, get_current_focus_screen());
                break;

            default:
                continue;
            }

            if ((last_event_type != drivers::input_event_type::none) && (last_event_type != input_event.type_)) {
                flush_events();
            }

            switch (input_event.type_) {
            case drivers::input_event_type::touch:
                touch_shipper.add_new_event(guest_event);
                break;

            case drivers::input_event_type::key:
                key_shipper.add_new_event(guest_event);
                break;

            default:
                LOG_ERROR("Unknown driver event type {}", static_cast<int>(last_event_type));
                break;
            }

            last_event_type = input_event.type_;
        }

        // Flush remaining events.
        if (last_event_type != drivers::input_event_type::none) {
            flush_events();
        }

        sys->get_ntimer()->schedule_event(input_update_us - nn_late, input_handler_evt_, userdata);
    }

    static void create_screen_buffer_for_dsa(kernel_system *kern, epoc::screen *scr) {
        // Try to create memory chunk at kernel mapping, for DSA
        const std::string chunk_name = fmt::format("ScreenBuffer{}", scr->number);

        // Add more size to prevent crashing.
        const std::size_t max_chunk_size = (scr->size().x + 100) * scr->size().y * 4;

        // Create chunk with maximum size (32-bit)
        kernel::chunk *buffer = kern->create<kernel::chunk>(kern->get_memory_system(), nullptr, chunk_name, 0,
            static_cast<address>(max_chunk_size), max_chunk_size, prot::read_write,
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
        crr->set_screen_mode(get_graphics_driver(), crr->crr_mode);

        create_screen_buffer_for_dsa(kern, crr);

        // Create other available screens. Plugged in screen later will be created explicitly
        for (std::size_t i = 0; i < screen_configs.size() - 1; i++) {
            crr->next = new epoc::screen(1, get_screen_config(1));

            crr = crr->next;

            if ((crr->size().x != -1) && (crr->size().y != -1)) {
                crr->set_screen_mode(get_graphics_driver(), crr->crr_mode);
                create_screen_buffer_for_dsa(kern, crr);
            }
        }

        // Set default focus screen to be the first
        focus_screen_ = screens;
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

    epoc::window_group *window_server::get_group_from_id(const epoc::ws::uid id) {
        epoc::screen *current = screens;

        while (current) {
            epoc::window_group *group = reinterpret_cast<epoc::window_group *>(current->root->child);
            while (group && group->id != id) {
                group = reinterpret_cast<epoc::window_group *>(group->sibling);
            }

            if (group && group->id == id) {
                return group;
            }

            current = current->next;
        }

        return nullptr;
    }

    void window_server::do_base_init() {
        load_wsini();
        parse_wsini();
        init_screens();

        // Schedule an event which will frequently queries input from host
        ntimer *timing = sys->get_ntimer();

        input_handler_evt_ = timing->register_event("ws_serv_input_update_event", [this](std::uint64_t userdata, int cycles_late) {
            handle_inputs_from_driver(userdata, cycles_late);
        });

        timing->schedule_event(input_update_us, input_handler_evt_, reinterpret_cast<std::uint64_t>(this));

        loaded = true;
    }

    void window_server::connect(service::ipc_context &ctx) {
        std::optional<std::uint32_t> version = ctx.get_arg<std::uint32_t>(0);
        epoc::version v = *reinterpret_cast<epoc::version *>(&version.value());

        clients.emplace(ctx.msg->msg_session->unique_id(),
            std::make_unique<epoc::window_server_client>(ctx.msg->msg_session, ctx.msg->own_thr, v));

        server::connect(ctx);
    }

    void window_server::disconnect(service::ipc_context &ctx) {
        clients.erase(ctx.msg->msg_session->unique_id());
        server::disconnect(ctx);
    }

    void window_server::init(service::ipc_context &ctx) {
        if (!loaded) {
            do_base_init();
        }

        ctx.set_request_status(ctx.msg->msg_session->unique_id());
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
        if (ctx.msg->function & EWservMessAsynchronousService) {
            switch (ctx.msg->function & ~EWservMessAsynchronousService) {
            case ws_cl_op_redraw_ready: {
                epoc::notify_info info;
                info.requester = ctx.msg->own_thr;
                info.sts = ctx.msg->request_sts;

                clients[ctx.msg->msg_session->unique_id()]->add_redraw_listener(info);

                break;
            }

            // Notify when an event is ringing, means that whenever
            // is occured within an object that belongs to a client that
            // created by the same thread as the requester, that requester
            // will be notify
            case ws_cl_op_event_ready: {
                epoc::notify_info info;
                info.requester = ctx.msg->own_thr;
                info.sts = ctx.msg->request_sts;

                clients[ctx.msg->msg_session->unique_id()]->add_event_listener(info);

                break;
            }

            default: {
                LOG_TRACE("UNHANDLE ASYNC OPCODE: {}",
                    ctx.msg->function & ~EWservMessAsynchronousService);

                break;
            }
            }
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
            if (win && win->type != epoc::window_kind::group && ((accept_pri == -1) || (win->priority == accept_pri))) {
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
            LOG_TRACE("Screen number {} doesnt exist", scr_num);
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
            LOG_TRACE("Screen number {} doesnt exist", scr_num);
            return 0;
        }

        window_group_tree_moonwalker walker(scr, ids, 0, pri, max);
        scr->root->child->walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children_and_previous_siblings);

        return walker.total;
    }

    std::uint32_t window_server::get_window_group_list_and_chain(epoc::window_group_chain_info *infos, const std::uint32_t max, const int pri, const int scr_num) {
        epoc::screen *scr = get_screen(scr_num);

        if (!scr) {
            LOG_TRACE("Screen number {} doesnt exist", scr_num);
            return false;
        }

        window_group_tree_moonwalker walker(scr, infos, window_group_tree_moonwalker::FLAGS_GET_CHAIN, pri, max);
        scr->root->child->walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children_and_previous_siblings);

        return walker.total;
    }

    fbs_server *window_server::get_fbs_server() {
        if (!fbss) {
            fbss = reinterpret_cast<fbs_server *>(&(*sys->get_kernel_system()->get_by_name<service::server>("!Fontbitmapserver")));
        }

        return fbss;
    }

    epoc::bitwise_bitmap *window_server::get_bitmap(const std::uint32_t h) {
        fbsbitmap *bmp = get_fbs_server()->get<fbsbitmap>(h);
        if (bmp) {
            return bmp->bitmap_;
        }

        return nullptr;
    }
}
