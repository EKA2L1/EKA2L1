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

#include <epoc/services/window/op.h>
#include <epoc/services/window/window.h>

#include <epoc/services/window/classes/gctx.h>
#include <epoc/services/window/classes/plugins/animdll.h>
#include <epoc/services/window/classes/plugins/clickdll.h>
#include <epoc/services/window/classes/plugins/sprite.h>
#include <epoc/services/window/classes/scrdvc.h>
#include <epoc/services/window/classes/winbase.h>
#include <epoc/services/window/classes/wingroup.h>
#include <epoc/services/window/classes/winuser.h>
#include <epoc/services/window/classes/wsobj.h>
#include <epoc/services/fbs/fbs.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/rgb.h>

#include <e32err.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/timing.h>
#include <epoc/vfs.h>

#include <optional>
#include <string>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

namespace eka2l1::epoc {    
    bool operator < (const event_capture_key_notifier &lhs, const event_capture_key_notifier &rhs) {
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

    window_server_client::window_server_client(service::session *guest_session, kernel::thread *own_thread)
        : guest_session(guest_session)
        , client_thread(own_thread)
        , uid_counter(0) {
        add_object(std::make_shared<epoc::window>(this));
        root = std::reinterpret_pointer_cast<epoc::window>(objects.back());
    }

    void window_server_client::execute_commands(service::ipc_context &ctx, std::vector<ws_cmd> cmds) {
        for (const auto &cmd : cmds) {
            if (cmd.obj_handle == guest_session->unique_id()) {
                execute_command(ctx, cmd);
            } else {
                if (auto obj = get_object(cmd.obj_handle)) {
                    obj->execute_command(ctx, cmd);
                }
            }
        }
    }

    std::uint32_t window_server_client::queue_redraw(epoc::window_user *user, const eka2l1::rect &r) {
        // Calculate the priority
        return redraws.queue_event(epoc::redraw_event{ user->id, r.top, 
        vec2(r.top.x + r.size.x, r.top.y + r.size.y) },
            user->redraw_priority());
    }

    std::uint32_t window_server_client::add_object(window_client_obj_ptr obj) {
        objects.push_back(std::move(obj));
        objects.back()->id = ++uid_counter;

        return (objects.size() & 0xFFFF) | (uid_counter.load()) << 16;
    }

    window_client_obj_ptr window_server_client::get_object(const std::uint32_t handle) {
        const std::uint32_t idx = handle & 0xFFFF;

        if (idx > objects.size() || idx == 0) {
            LOG_WARN("Object handle is invalid {}", handle);
            return nullptr;
        }

        return objects[idx - 1];
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

    void window_server_client::create_screen_device(service::ipc_context &ctx, ws_cmd cmd) {
        LOG_INFO("Create screen device.");

        ws_cmd_screen_device_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);

        epoc::screen_device_ptr device
            = std::make_shared<epoc::screen_device>(
                this, header->num_screen, ctx.sys->get_graphic_driver_client());

        if (!primary_device) {
            primary_device = device;
        }

        init_device(root);
        devices.push_back(device);

        ctx.set_request_status(add_object(device));
    }

    void window_server_client::init_device(epoc::window_ptr &win) {
        if (win->type == epoc::window_kind::group) {
            epoc::window_group_ptr group_win = std::reinterpret_pointer_cast<epoc::window_group>(
                win);

            if (!group_win->dvc) {
                group_win->dvc = primary_device;
            }
        }

        for (auto &child_win : win->childs) {
            init_device(child_win);
        }
    }

    void window_server_client::restore_hotkey(service::ipc_context &ctx, ws_cmd cmd) {
        THotKey key = *reinterpret_cast<THotKey *>(cmd.data_ptr);

        LOG_WARN("Unknown restore key op.");
    }

    void window_server_client::create_window_group(service::ipc_context &ctx, ws_cmd cmd) {
        ws_cmd_window_group_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);
        int device_handle = header->screen_device_handle;

        epoc::screen_device_ptr device_ptr;

        if (device_handle <= 0) {
            device_ptr = primary_device;
        } else {
            device_ptr = std::reinterpret_pointer_cast<epoc::screen_device>(get_object(device_handle));
        }

        if (!device_ptr) {
            device_ptr = primary_device;
        }

        epoc::window_ptr group = std::make_shared<epoc::window_group>(this, device_ptr);
        epoc::window_ptr parent_group = find_window_obj(root, header->parent_id);

        if (!parent_group) {
            LOG_WARN("Unable to find parent for new group with ID = 0x{:x}. Use root", header->parent_id);
            parent_group = root;
        }

        group->parent = &(*parent_group);

        if (last_group) {
            last_group->next_sibling = std::reinterpret_pointer_cast<epoc::window_group>(group);
        }

        parent_group->childs.push(group);
        device_ptr->windows.push_back(&(*group));

        if (header->focus) {
            std::reinterpret_pointer_cast<epoc::window_group>(group)->flags |= window_group::focus_receiveable;

            device_ptr->update_focus(nullptr);
        }

        last_group = std::reinterpret_pointer_cast<epoc::window_group>(group);
        total_group++;

        std::uint32_t id = add_object(group);

        ctx.set_request_status(id);
    }

    void window_server_client::create_window_base(service::ipc_context &ctx, ws_cmd cmd) {
        ws_cmd_window_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);

        epoc::window_ptr parent = find_window_obj(root, header->parent);

        if (!parent) {
            LOG_WARN("Unable to find parent for new window with ID = 0x{:x}. Use root", header->parent);
            parent = root;
        }

        if (parent->type != window_kind::group) {
            LOG_ERROR("The parent of window user must be a group!");
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::shared_ptr<epoc::window_user> win = std::make_shared<epoc::window_user>(this, parent->dvc,
            header->win_type, header->dmode);

        win->driver_win_id = std::reinterpret_pointer_cast<epoc::window_group>(parent)->get_driver()->
            create_window(eka2l1::vec2(200, 200), 0, true);

        win->parent = &(*parent);
        parent->childs.push(win);

        ctx.set_request_status(add_object(win));
    }

    void window_server_client::create_graphic_context(service::ipc_context &ctx, ws_cmd cmd) {
        std::shared_ptr<epoc::graphic_context> gcontext = std::make_shared<epoc::graphic_context>(this);

        ctx.set_request_status(add_object(gcontext));
    }

    void window_server_client::create_sprite(service::ipc_context &ctx, ws_cmd cmd) {
        ws_cmd_create_sprite_header *sprite_header = reinterpret_cast<decltype(sprite_header)>(cmd.data_ptr);
        epoc::window_ptr win = nullptr;

        if (sprite_header->window_handle <= 0) {
            LOG_WARN("Window handle is invalid, use root");
            win = root;
        } else {
            win = std::reinterpret_pointer_cast<epoc::window>(get_object(sprite_header->window_handle));
        }

        std::shared_ptr<epoc::sprite> spr = std::make_shared<epoc::sprite>(this, std::move(win), sprite_header->base_pos);
        ctx.set_request_status(add_object(spr));
    }

    void window_server_client::create_anim_dll(service::ipc_context &ctx, ws_cmd cmd) {
        int dll_name_length = *reinterpret_cast<int *>(cmd.data_ptr);
        char16_t *dll_name_ptr = reinterpret_cast<char16_t *>(
            reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + sizeof(int));

        std::u16string dll_name(dll_name_ptr, dll_name_length);

        LOG_TRACE("Create ANIMDLL for {}, stubbed object", common::ucs2_to_utf8(dll_name));

        std::shared_ptr<epoc::anim_dll> animdll = std::make_shared<epoc::anim_dll>(this);
        ctx.set_request_status(add_object(animdll));
    }

    void window_server_client::create_click_dll(service::ipc_context &ctx, ws_cmd cmd) {
        LOG_TRACE("Create CLICKDLL (button click sound plugin), stubbed object");

        std::shared_ptr<epoc::click_dll> clickdll = std::make_shared<epoc::click_dll>(this);
        ctx.set_request_status(add_object(clickdll));
    }

    epoc::window_ptr window_server_client::find_window_obj(epoc::window_ptr &root, std::uint32_t id) {
        return std::reinterpret_pointer_cast<epoc::window>(get_object(id));
    }

    // This handle both sync and async
    void window_server_client::execute_command(service::ipc_context &ctx, ws_cmd cmd) {
        switch (cmd.header.op) {
        // Gets the total number of window groups with specified priority currently running
        // in the window server.
        case EWsClOpNumWindowGroups: {
            int pri = *reinterpret_cast<int *>(cmd.data_ptr);
            std::uint32_t total = get_ws().get_total_window_groups(pri);

            ctx.set_request_status(static_cast<int>(total));
            break;
        }

        // Gets the total number of window groups currently running in the window server.
        case EWsClOpNumWindowGroupsAllPriorities: {
            ctx.set_request_status(static_cast<int>(get_ws().get_total_window_groups(-1)));
            break;
        }

        case EWsClOpSendEventToWindowGroup: {
            ws_cmd_send_event_to_window_group *evt = reinterpret_cast<decltype(evt)>(cmd.data_ptr);
            queue_event(evt->evt);

            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsClOpComputeMode: {
            LOG_TRACE("Setting compute mode not supported, instead stubbed");
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsClOpSetPointerCursorMode: {
            // TODO: Check errors
            if (get_ws().get_focus() && get_ws().get_focus()->client == this) {
                get_ws().cursor_mode() = *reinterpret_cast<epoc::pointer_cursor_mode *>(cmd.data_ptr);
                ctx.set_request_status(KErrNone);

                break;
            }

            ctx.set_request_status(KErrPermissionDenied);

            break;
        }

        case EWsClOpGetWindowGroupClientThreadId: {
            std::uint32_t group_id = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
            epoc::window_ptr win = find_window_obj(root, group_id);

            if (!win || win->type != window_kind::group) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            std::uint32_t thr_id = win->client->get_client()->unique_id();

            ctx.write_arg_pkg<std::uint32_t>(reply_slot, thr_id);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsClOpGetRedraw: {
            auto evt = redraws.get_evt_opt();

            if (!evt) {
                ctx.set_request_status(KErrNotFound);
                break;
            }

            ctx.write_arg_pkg<epoc::redraw_event>(reply_slot, *evt);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsClOpGetEvent: {
            auto evt = events.get_event();

            // Allow the context to shrink if needed, since the struct certainly got larger as Symbian
            // grows. S^3 has advance pointer struct which along takes 56 bytes buffer.
            ctx.write_arg_pkg<epoc::event>(reply_slot, evt, nullptr, true);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsClOpCreateScreenDevice:
            create_screen_device(ctx, cmd);
            break;

        case EWsClOpCreateWindowGroup:
            create_window_group(ctx, cmd);
            break;

        case EWsClOpCreateWindow:
            create_window_base(ctx, cmd);
            break;

        case EWsClOpRestoreDefaultHotKey:
            restore_hotkey(ctx, cmd);
            break;

        case EWsClOpCreateGc:
            create_graphic_context(ctx, cmd);
            break;

        case EWsClOpCreateSprite:
            create_sprite(ctx, cmd);
            break;

        case EWsClOpCreateAnimDll:
            create_anim_dll(ctx, cmd);
            break;

        case EWsClOpCreateClick:
            create_click_dll(ctx, cmd);
            break;

        case EWsClOpEventReady:
            break;

        case EWsClOpGetFocusWindowGroup: {
            // TODO: Epoc < 9
            if (cmd.header.cmd_len == 0) {
                ctx.set_request_status(primary_device->focus->id);
                break;
            }

            int screen_num = *reinterpret_cast<int *>(cmd.data_ptr);

            auto dvc_ite = std::find_if(devices.begin(), devices.end(),
                [screen_num](const epoc::screen_device_ptr &dvc) { return dvc->screen == screen_num; });

            if (dvc_ite == devices.end()) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            ctx.set_request_status((*dvc_ite)->focus->id);
            break;
        }

        case EWsClOpFindWindowGroupIdentifier: {
            ws_cmd_find_window_group_identifier *find_info = reinterpret_cast<decltype(find_info)>(cmd.data_ptr);

            epoc::window_group_ptr group = nullptr;

            if (find_info->parent_identifier) {
                group = std::reinterpret_pointer_cast<epoc::window_group>(find_window_obj(root, find_info->parent_identifier));
            } else {
                LOG_TRACE("Parent identifier not specified, use root window group");
                group = std::reinterpret_pointer_cast<epoc::window_group>(
                    *root->childs.begin());
            }

            if (!group || group->type != window_kind::group) {
                ctx.set_request_status(KErrNotFound);
                break;
            }

            char16_t *win_group_name_ptr = reinterpret_cast<char16_t *>(find_info + 1);
            std::u16string win_group_name(win_group_name_ptr, find_info->length);

            bool found = false;

            for (; group; group = group->next_sibling) {
                if (common::compare_ignore_case(group->name.substr(find_info->offset),
                        win_group_name)
                    == 0) {
                    ctx.set_request_status(group->id);
                    found = true;

                    break;
                }
            }

            if (!found) {
                ctx.set_request_status(KErrNotFound);
            }

            break;
        }

        case EWsClOpGetWindowGroupNameFromIdentifier: {
            ws_cmd_get_window_group_name_from_id *find_info = reinterpret_cast<decltype(find_info)>(cmd.data_ptr);

            epoc::window_group_ptr group = std::reinterpret_pointer_cast<epoc::window_group>(
                find_window_obj(root, find_info->id));

            if (!group || group->type != window_kind::group) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            if (group->name.length() == 0) {
                ctx.set_request_status(KErrNotReady);
                break;
            }

            std::uint32_t len_to_write = std::min(static_cast<std::uint32_t>(find_info->max_len),
                static_cast<std::uint32_t>(group->name.length()));

            std::u16string to_write = group->name.substr(0, len_to_write);

            ctx.write_arg(reply_slot, to_write);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsClOpWindowGroupListAndChain:
        case EWsClOpWindowGroupListAndChainAllPriorities: {
            ws_cmd_window_group_list *list_req = reinterpret_cast<decltype(list_req)>(cmd.data_ptr);

            std::vector<std::uint32_t> ids;
            get_ws().get_window_group_list(ids, list_req->count, cmd.header.op == EWsClOpWindowGroupListAndChainAllPriorities ? 
                -1 : list_req->priority, list_req->screen_num);

            ctx.write_arg_pkg(reply_slot, reinterpret_cast<std::uint8_t *>(&ids[0]),
                static_cast<std::uint32_t>(ids.size() * sizeof(std::uint32_t)));

            ctx.set_request_status(static_cast<int>(ids.size()));
            break;
        }

        default:
            LOG_INFO("Unimplemented ClOp: 0x{:x}", cmd.header.op);
        }
    }

    ws::uid window_server_client::add_capture_key_notifier_to_server(epoc::event_capture_key_notifier &notifier) {
        const ws::uid id = ++get_ws().key_capture_uid_counter;
        notifier.id = id;

        window_server::key_capture_request_queue &rqueue =  get_ws().key_capture_requests[notifier.keycode_];

        if (!rqueue.empty() && notifier.pri_ == 0) {
            notifier.pri_ = rqueue.top().pri_ + 1;
        }

        rqueue.push(std::move(notifier));

        return id;
    }

    std::uint32_t window_server_client::get_total_window_groups(const int pri, const int scr_num) {
        if (pri == -1) {
            return total_group;
        }

        epoc::window_group_ptr gr = nullptr;
        std::uint32_t total = 0;

        for (auto &child : root->childs) {
            if (child->type == window_kind::group) {
                gr = std::reinterpret_pointer_cast<epoc::window_group>(child);
                break;
            }
        }

        if (!gr) {
            LOG_TRACE("No window group detected");
            return 0;
        }

        for (; gr != nullptr; gr = gr->next_sibling) {
            if (gr->priority == pri && gr->dvc->screen == scr_num) {
                total++;
            }
        }

        return total;
    }

    bool window_server_client::get_window_group_list(std::vector<std::uint32_t> &ids, const std::uint32_t max,
        const int pri, const int scr_num) {
        epoc::window_group_ptr gr = nullptr;
        std::uint32_t total = 0;

        for (auto &child : root->childs) {
            if (child->type == window_kind::group) {
                gr = std::reinterpret_pointer_cast<epoc::window_group>(child);
                break;
            }
        }

        if (!gr) {
            LOG_TRACE("No window group detected");
            return true;
        }

        for (; (gr != nullptr) && (static_cast<std::uint32_t>(ids.size()) <= max); gr = gr->next_sibling) {
            if ((pri == -1 || (gr->priority == pri)) && gr->dvc->screen == scr_num) {
                ids.push_back(gr->id);
            }
        }

        if (ids.size() == max) {
            return false;
        }

        return true;
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
        common::ini_node_ptr screen_node = nullptr;
        int total_screen = 0;

        do {
            std::string screen_key = "SCREEN";
            screen_key += std::to_string(total_screen);
            screen_node = ws_config.find(screen_key.c_str());

            if (!screen_node || !common::ini_section::is_my_type(screen_node)) {
                break;
            }

            total_screen++;
            epoc::config::screen scr;

            scr.screen_number = total_screen - 1;

            int total_mode = 0;

            do {
                std::string screen_mode_width_key = "SCR_WIDTH";
                screen_mode_width_key += std::to_string(total_mode + 1);

                common::ini_node_ptr mode_width = screen_node->get_as<common::ini_section>()->find(screen_mode_width_key.c_str());

                if (!mode_width) {
                    break;
                }

                std::string screen_mode_height_key = "SCR_HEIGHT";
                screen_mode_height_key += std::to_string(total_mode + 1);

                common::ini_node_ptr mode_height = screen_node->get_as<common::ini_section>()->find(screen_mode_height_key.c_str());

                total_mode++;

                epoc::config::screen_mode scr_mode;
                scr_mode.screen_number = total_screen - 1;
                scr_mode.mode_number = total_mode;

                mode_width->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&scr_mode.size.x),
                    1, 0);
                mode_height->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&scr_mode.size.y),
                    1, 0);

                std::string screen_mode_rot_key = "SCR_ROTATION";
                screen_mode_rot_key += std::to_string(total_mode);

                common::ini_node_ptr mode_rot = screen_node->get_as<common::ini_section>()->find(screen_mode_rot_key.c_str());

                mode_rot->get_as<common::ini_pair>()->get(reinterpret_cast<std::uint32_t *>(&scr_mode.rotation),
                    1, 0);

                scr.modes.push_back(scr_mode);
            } while (true);

            screens.push_back(scr);
        } while (screen_node != nullptr);
    }

    window_server::window_server(system *sys)
        : service::server(sys, "!Windowserver", true, true) 
        , bmp_cache(sys->get_kernel_system()) {
        REGISTER_IPC(window_server, init, EWservMessInit,
            "Ws::Init");
        REGISTER_IPC(window_server, send_to_command_buffer, EWservMessCommandBuffer,
            "Ws::CommandBuffer");
        REGISTER_IPC(window_server, send_to_command_buffer, EWservMessSyncMsgBuf,
            "Ws::MessSyncBuf");
    }

    constexpr std::int64_t input_update_ticks = 10000;

    static void make_key_event(drivers::input_event &driver_evt_, epoc::event &guest_evt_) {
        // For up and down events, the keycode will always be 0
        // We still have to fill valid value for event_code::key
        guest_evt_.key_evt_.code = 0;
        guest_evt_.type = (driver_evt_.key_.state_ == drivers::key_state::pressed) ? epoc::event_code::key_down : epoc::event_code::key_up;
        guest_evt_.key_evt_.scancode = static_cast<std::uint32_t>(driver_evt_.key_.code_);
        guest_evt_.key_evt_.repeats = 0;            // TODO?
    }

    void window_server::handle_inputs_from_driver(std::uint64_t userdata, int cycles_late) {
        if (!focus_) {
            sys->get_timing_system()->schedule_event(input_update_ticks - cycles_late, input_handler_evt_, userdata);
            return;
        }

        std::vector<drivers::input_event> driver_input_events;

        {
            const std::lock_guard<std::mutex> guard(idriver_cli_->connect_lock);

            if (idriver_cli_->is_disconnected()) {
                return;
            }

            // Lock it first
            idriver_cli_->lock();
            const std::uint32_t total_evt = idriver_cli_->total();

            if (total_evt == 0) {
                idriver_cli_->release();
                sys->get_timing_system()->schedule_event(input_update_ticks - cycles_late, input_handler_evt_, userdata);

                return;
            }

            driver_input_events.resize(total_evt);
            idriver_cli_->get(&driver_input_events[0], total_evt);
            
            // Release the lock
            idriver_cli_->release();   
        }
        
        std::vector<epoc::event> guest_events;
        guest_events.resize(driver_input_events.size());

        // Processing the events, translate them to cool things
        for (std::size_t i = 0; i < driver_input_events.size(); i++) {
            epoc::event extra_key_evt;
            
            switch (driver_input_events[i].type_) {
            case drivers::input_event_type::key: {
                make_key_event(driver_input_events[i], guest_events[i]);
                    
                extra_key_evt = guest_events[i];
                extra_key_evt.type = epoc::event_code::key;
                extra_key_evt.key_evt_.code = epoc::map_scancode_to_keycode(
                    static_cast<TStdScanCode>(guest_events[i].key_evt_.scancode));

                break;
            }

            default: 
                break;
            }

            // Report to the focused window first
            guest_events[i].handle = focus_->owner_handle;    // TODO: this should work
            extra_key_evt.handle = focus_->owner_handle;    // TODO: this should work
            focus_->queue_event(guest_events[i]);
    
            // Send a key event also
            if (guest_events[i].type == epoc::event_code::key_down) {
                focus_->queue_event(extra_key_evt);
            }

            // Now we find all request from other windows and start doing horrible stuffs with it
            // Not so horrible though ... :) Don't worry, they won't get hurt!
            if (driver_input_events[i].type_ == drivers::input_event_type::key) {
                key_capture_request_queue &rqueue = key_capture_requests[extra_key_evt.key_evt_.code];

                epoc::ws::uid top_id;

                for (auto ite = rqueue.end(); ite != rqueue.begin(); ite--) {
                    if (ite->user->id == focus_->id) {
                        break;
                    }

                    if (ite == rqueue.end()) {
                        top_id = ite->user->id;
                    }

                    if (ite->user->id != top_id) {
                        break;
                    }

                    switch (ite->type_) {
                    case epoc::event_key_capture_type::normal: {
                        extra_key_evt.handle = ite->user->owner_handle;    // TODO: this should work
                        ite->user->queue_event(extra_key_evt);

                        break;
                    }

                    case epoc::event_key_capture_type::up_and_downs: {
                        guest_events[i].handle = ite->user->owner_handle;    // TODO: this should work
                        ite->user->queue_event(guest_events[i]);

                        break;
                    }

                    default:
                        break;
                    }
                }
            }
        }

        sys->get_timing_system()->schedule_event(input_update_ticks - cycles_late, input_handler_evt_, userdata);
    }
    
    void window_server::do_base_init() {
        load_wsini();
        parse_wsini();

        idriver_cli_ = sys->get_input_driver_client();

        // Schedule an event which will frequently queries input from host
        timing_system *timing = sys->get_timing_system();

        input_handler_evt_ = timing->register_event("InputUpdateEvent", [this](std::uint64_t userdata, int cycles_late) {
            handle_inputs_from_driver(userdata, cycles_late);
        });

        timing->schedule_event(input_update_ticks, input_handler_evt_, reinterpret_cast<std::uint64_t>(this));

        loaded = true;
    }
    
    void window_server::init(service::ipc_context &ctx) {
        if (!loaded) {
            do_base_init();
        }

        clients.emplace(ctx.msg->msg_session->unique_id(),
            std::make_shared<epoc::window_server_client>(ctx.msg->msg_session, ctx.msg->own_thr));

        ctx.set_request_status(ctx.msg->msg_session->unique_id());
    }

    epoc::config::screen &window_server::get_current_focus_screen_config() {
        int num = 0;
        if (focus_) {
            num = focus_->dvc->screen;
        }

        if (!loaded) {
            do_base_init();
        }

        return screens[num];
    }
    
    void window_server::send_to_command_buffer(service::ipc_context &ctx) {
        clients[ctx.msg->msg_session->unique_id()]->parse_command_buffer(ctx);
    }

    void window_server::on_unhandled_opcode(service::ipc_context &ctx) {
        if (ctx.msg->function & EWservMessAsynchronousService) {
            switch (ctx.msg->function & ~EWservMessAsynchronousService) {
            case EWsClOpRedrawReady: {
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
            case EWsClOpEventReady: {
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

    std::uint32_t window_server::get_total_window_groups(const int pri, const int scr_num) {
        std::uint32_t total = 0;

        for (auto &cli : clients) {
            total += cli.second->get_total_window_groups(pri, scr_num);
        }

        return total;
    }

    void window_server::get_window_group_list(std::vector<std::uint32_t> &ids, const std::uint32_t max,
        const int pri, const int scr_num) {
        for (auto &cli: clients) {
            if (!cli.second->get_window_group_list(ids, max, pri, scr_num)) {
                return;
            }
        }
    }

    epoc::bitwise_bitmap *window_server::get_bitmap(const std::uint32_t h) {
        if (!fbss) {
            fbss = reinterpret_cast<fbs_server*>(&(*sys->get_kernel_system()->
                get_by_name<service::server>("!Fontbitmapserver")));
        }

        return fbss->get<fbsbitmap>(h)->bitmap_;
    }
}
