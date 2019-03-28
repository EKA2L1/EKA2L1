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
#include <unordered_set>
#include <type_traits>
#include <vector>

#include <common/ini.h>
#include <common/queue.h>
#include <common/vecx.h>

#include <epoc/services/window/bitmap_cache.h>
#include <epoc/services/window/classes/config.h>
#include <epoc/services/window/common.h>
#include <epoc/services/window/fifo.h>
#include <epoc/services/window/opheader.h>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

#include <epoc/ptr.h>
#include <epoc/services/server.h>
#include <epoc/utils/des.h>

namespace eka2l1 {
    class window_server;
    class fbs_server;

    namespace drivers {
        class input_driver_client;
    }

    using input_driver_client_ptr = std::shared_ptr<drivers::input_driver_client>;
}

namespace eka2l1::epoc {
    struct window;

    struct event_mod_notifier {
        event_modifier what;
        event_control when;
    };

    struct event_notifier_base {
        epoc::window *user;
    };

    struct event_mod_notifier_user : public event_notifier_base {
        event_mod_notifier notifier;
    };

    struct event_screen_change_user : public event_notifier_base {
    };

    struct event_error_msg_user : public event_notifier_base {
        event_control when;
    };

    enum class event_key_capture_type {
        normal,
        up_and_downs
    };

    struct event_capture_key_notifier: public event_notifier_base  {
        event_key_capture_type type_;
        std::uint32_t keycode_;
        std::uint32_t modifiers_mask_;
        std::uint32_t modifiers_;
        std::uint32_t pri_;

        std::uint32_t id;
    };

    bool operator < (const event_capture_key_notifier &lhs, const event_capture_key_notifier &rhs);
    
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

    namespace ws {
        using uid = std::uint32_t;
    };

    class window_server_client {
    public:
        template <typename T>
        struct event_nof_hasher {
            std::size_t operator() (const T &evt_nof) const {
                return evt_nof.user->id;
            }
        };

        template <typename T>
        struct event_nof_comparer {
            bool operator() (const T &lhs, const T &rhs) const {
                return lhs.user == rhs.user;
            }
        };

        template <typename T>
        using nof_container = std::unordered_set<T, event_nof_hasher<T>, event_nof_comparer<T>>;

    private:
        friend struct window_client_obj;
        friend struct window;
        friend struct window_group;

        service::session *guest_session;

        std::atomic<ws::uid> uid_counter;

        std::vector<window_client_obj_ptr> objects;
        std::vector<epoc::screen_device_ptr> devices;

        epoc::screen_device_ptr primary_device;
        epoc::window_ptr root;

        eka2l1::kernel::thread *client_thread;
        eka2l1::epoc::window_group_ptr last_group;

        epoc::redraw_fifo redraws;
        epoc::event_fifo events;

        std::mutex ws_client_lock;

        nof_container<epoc::event_mod_notifier_user> mod_notifies;
        nof_container<epoc::event_screen_change_user> screen_changes;
        nof_container<epoc::event_error_msg_user> error_notifies;
        
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

        void execute_command(service::ipc_context &ctx, ws_cmd cmd);
        void execute_commands(service::ipc_context &ctx, std::vector<ws_cmd> cmds);
        void parse_command_buffer(service::ipc_context &ctx);

        std::uint32_t add_object(window_client_obj_ptr obj);
        window_client_obj_ptr get_object(const std::uint32_t handle);

        bool delete_object(const std::uint32_t handle);

        explicit window_server_client(service::session *guest_session,
            kernel::thread *own_thread);

        eka2l1::window_server &get_ws() {
            return *(std::reinterpret_pointer_cast<window_server>(guest_session->get_server()));
        }

        kernel::thread *get_client() {
            return client_thread;
        }

        std::uint32_t queue_redraw(epoc::window_user *user, const eka2l1::rect &r);

        std::uint32_t queue_event(const event &evt) {
            return events.queue_event(evt);
        }

        void deque_redraw(const std::uint32_t handle) {
            redraws.cancel_event_queue(handle);
        }

        ws::uid add_capture_key_notifier_to_server(epoc::event_capture_key_notifier &notifier);
        
        // We have been blessed with so much reflection that it's actually seems evil now.
        template <typename T>
        constexpr ws::uid add_event_notifier(T &evt) {
            const std::lock_guard guard_(ws_client_lock);

            if constexpr (std::is_same_v<T, epoc::event_mod_notifier_user>) {
                mod_notifies.emplace(std::move(evt));
                return static_cast<ws::uid>(mod_notifies.size());
            } else if constexpr (std::is_same_v<T, epoc::event_screen_change_user>) {
                screen_changes.emplace(std::move(evt));
                return static_cast<ws::uid>(screen_changes.size());
            } else if constexpr (std::is_same_v<T, epoc::event_error_msg_user>) {
                error_notifies.emplace(std::move(evt));
                return static_cast<ws::uid>(error_notifies.size());
            } else if constexpr (std::is_same_v<T, epoc::event_capture_key_notifier>) {
                return add_capture_key_notifier_to_server(evt);
            } else {
                throw std::runtime_error("Unsupported event notifier type!");
            }
        }
    };

    epoc::graphics_orientation number_to_orientation(int rot);
}

namespace eka2l1 {
    class window_server : public service::server {
    public:
        using key_capture_request_queue = cp_queue<epoc::event_capture_key_notifier>;

    private:
        friend class epoc::window_server_client;

        std::unordered_map<std::uint64_t, std::shared_ptr<epoc::window_server_client>>
            clients;

        common::ini_file ws_config;
        bool loaded{ false };

        std::atomic<epoc::ws::uid> key_capture_uid_counter {0};

        std::vector<epoc::config::screen> screens;
        std::unordered_map<epoc::ws::uid, key_capture_request_queue> key_capture_requests;

        epoc::window_group_ptr focus_;
        epoc::pointer_cursor_mode cursor_mode_;

        void init(service::ipc_context ctx);
        void send_to_command_buffer(service::ipc_context ctx);

        void on_unhandled_opcode(service::ipc_context ctx) override;

        void load_wsini();
        void parse_wsini();

        input_driver_client_ptr idriver_cli_;
        int input_handler_evt_;

        void handle_inputs_from_driver(std::uint64_t userdata, int cycles_late);

        epoc::bitmap_cache bmp_cache;
        fbs_server *fbss { nullptr };

    public:
        window_server(system *sys);

        epoc::bitwise_bitmap *get_bitmap(const std::uint32_t h);
        epoc::bitmap_cache *get_bitmap_cache() {
            return &bmp_cache;
        }

        epoc::config::screen &get_current_focus_screen_config();

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

        void do_base_init();

        epoc::pointer_cursor_mode &cursor_mode() {
            return cursor_mode_;
        }

        epoc::window_group_ptr &get_focus() {
            return focus_;
        }

        epoc::config::screen &get_screen_config(int num) {
            assert(num < screens.size());
            return screens[num];
        }
    };

}
