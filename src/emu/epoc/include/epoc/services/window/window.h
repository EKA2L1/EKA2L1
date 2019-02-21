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

#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include <common/ini.h>
#include <common/queue.h>

#include <epoc/services/window/common.h>
#include <epoc/services/window/opheader.h>
#include <epoc/services/window/fifo.h>
#include <epoc/services/window/classes/config.h>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

#include <epoc/ptr.h>
#include <epoc/services/server.h>
#include <epoc/utils/des.h>

namespace eka2l1 {
    class window_server;
}

namespace eka2l1::epoc {
    struct window;
    
    struct event_mod_notifier {
        event_modifier what;
        event_control when;
    };

    struct event_mod_notifier_user {
        event_mod_notifier notifier;
        epoc::window *user;
    };

    struct event_screen_change_user {
        epoc::window *user;
    };

    struct event_error_msg_user {
        event_control when;
        epoc::window *user;
    };

    struct pixel_twips_and_rot {
        eka2l1::vec2 pixel_size;
        eka2l1::vec2 twips_size;
        graphics_orientation orientation;
    };

    struct pixel_and_rot {
        eka2l1::vec2 pixel_size;
        graphics_orientation orientation;
    };

    struct window_client_obj;
    using window_client_obj_ptr = std::shared_ptr<window_client_obj>;

    struct screen_device;
    using screen_device_ptr = std::shared_ptr<screen_device>;

    struct window;
    using window_ptr = std::shared_ptr<window>;

    struct window_group;
    using window_group_ptr = std::shared_ptr<window_group>;

    struct window_user;

    class window_server_client {
        friend struct window_client_obj;

        session_ptr guest_session;

        std::vector<window_client_obj_ptr> objects;
        std::vector<epoc::screen_device_ptr> devices;

        epoc::screen_device_ptr primary_device;
        epoc::window_ptr root;

        eka2l1::thread_ptr client_thread;
        eka2l1::epoc::window_group_ptr last_group;

        epoc::redraw_fifo redraws;
        epoc::event_fifo events;

        std::vector<epoc::event_mod_notifier_user> mod_notifies;
        std::vector<epoc::event_screen_change_user> screen_changes;
        std::vector<epoc::event_error_msg_user> error_notifies;

        void create_screen_device(service::ipc_context &ctx, ws_cmd cmd);
        void create_window_group(service::ipc_context &ctx, ws_cmd cmd);
        void create_window_base(service::ipc_context &ctx, ws_cmd cmd);
        void create_graphic_context(service::ipc_context &ctx, ws_cmd cmd);
        void create_anim_dll(service::ipc_context &ctx, ws_cmd cmd);
        void create_click_dll(service::ipc_context &ctx, ws_cmd cmd);
        void create_sprite(service::ipc_context &ctx, ws_cmd cmd);

        void restore_hotkey(service::ipc_context &ctx, ws_cmd cmd);

        void init_device(epoc::window_ptr &win);
        epoc::window_ptr find_window_obj(epoc::window_ptr &root, std::uint32_t id);

        std::uint32_t total_group{ 0 };

    public:
        void add_redraw_listener(notify_info nof) {
            redraws.set_listener(nof);
        }

        void add_event_listener(notify_info nof) {
            events.set_listener(nof);
        }

        std::uint32_t get_total_window_groups();
        std::uint32_t get_total_window_groups_with_priority(const std::uint32_t pri);

        void add_event_mod_notifier_user(epoc::event_mod_notifier_user nof);
        void add_event_screen_change_user(epoc::event_screen_change_user nof);
        void add_event_error_msg_user(epoc::event_error_msg_user nof);

        void execute_command(service::ipc_context &ctx, ws_cmd cmd);
        void execute_commands(service::ipc_context &ctx, std::vector<ws_cmd> cmds);
        void parse_command_buffer(service::ipc_context &ctx);

        std::uint32_t add_object(window_client_obj_ptr obj);
        window_client_obj_ptr get_object(const std::uint32_t handle);

        bool delete_object(const std::uint32_t handle);

        explicit window_server_client(session_ptr guest_session,
            thread_ptr own_thread);

        eka2l1::window_server &get_ws() {
            return *(std::reinterpret_pointer_cast<window_server>(guest_session->get_server()));
        }

        eka2l1::thread_ptr &get_client() {
            return client_thread;
        }

        std::uint32_t queue_redraw(epoc::window_user *user);

        std::uint32_t queue_event(const event &evt) {
            return events.queue_event(evt);
        }

        void deque_redraw(const std::uint32_t handle) {
            redraws.cancel_event_queue(handle);
        }
    };

    epoc::graphics_orientation number_to_orientation(int rot);
}

namespace eka2l1 {
    class window_server : public service::server {
        std::unordered_map<std::uint64_t, std::shared_ptr<epoc::window_server_client>>
            clients;

        common::ini_file ws_config;
        bool loaded{ false };

        std::vector<epoc::config::screen> screens;

        epoc::window_group_ptr focus_;
        epoc::pointer_cursor_mode cursor_mode_;

        void init(service::ipc_context ctx);
        void send_to_command_buffer(service::ipc_context ctx);

        void on_unhandled_opcode(service::ipc_context ctx) override;

        void load_wsini();
        void parse_wsini();

    public:
        window_server(system *sys);

        /**
         * \brief Get the number of window groups running in the server
         */
        std::uint32_t get_total_window_groups();

        /**
         * \brief Get the number of window groups running in the server
         *         with the specified priority.
         * 
         * \param pri The priority we want to count.
        */
        std::uint32_t get_total_window_groups_with_priority(const std::uint32_t pri);

        epoc::pointer_cursor_mode &cursor_mode() {
            return cursor_mode_;
        }

        epoc::window_group_ptr &focus() {
            return focus_;
        }

        epoc::config::screen &get_screen_config(int num) {
            assert(num < screens.size());
            return screens[num];
        }
    };

}