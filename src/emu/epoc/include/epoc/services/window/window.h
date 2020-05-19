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

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <common/ini.h>
#include <common/queue.h>
#include <common/vecx.h>

#include <epoc/services/window/bitmap_cache.h>
#include <epoc/services/window/classes/config.h>
#include <epoc/services/window/common.h>
#include <epoc/services/window/fifo.h>
#include <epoc/services/window/opheader.h>
#include <epoc/services/window/scheduler.h>
#include <epoc/services/window/screen.h>

#include <epoc/ptr.h>
#include <epoc/services/server.h>
#include <epoc/utils/des.h>
#include <epoc/utils/version.h>

#include <drivers/input/common.h>

namespace eka2l1 {
    class window_server;
    class fbs_server;

    namespace drivers {
        class graphics_driver;
    }
}

namespace eka2l1::epoc {
    struct window;
    struct window_key_shipper;

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

    struct event_capture_key_notifier : public event_notifier_base {
        event_key_capture_type type_;
        std::uint32_t keycode_;
        std::uint32_t modifiers_mask_;
        std::uint32_t modifiers_;
        std::uint32_t pri_;

        std::uint32_t id;
    };

    bool operator<(const event_capture_key_notifier &lhs, const event_capture_key_notifier &rhs);

    struct pixel_twips_and_rot {
        eka2l1::vec2 pixel_size;
        eka2l1::vec2 twips_size;
        graphics_orientation orientation;
    };

    struct pixel_and_rot {
        eka2l1::vec2 pixel_size;
        graphics_orientation orientation;
    };

    struct window_user;

    namespace ws {
        using uid = std::uint32_t;
    };

    struct window_client_obj;
    struct screen_device;

    using window_client_obj_ptr = std::unique_ptr<window_client_obj>;

    class window_server_client {
    public:
        template <typename T>
        struct event_nof_hasher {
            std::size_t operator()(const T &evt_nof) const {
                return evt_nof.user->id;
            }
        };

        template <typename T>
        struct event_nof_comparer {
            bool operator()(const T &lhs, const T &rhs) const {
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
        epoc::screen_device *primary_device;

        eka2l1::kernel::thread *client_thread;
        epoc::version cli_version;

        epoc::redraw_fifo redraws;
        epoc::event_fifo events;

        std::mutex ws_client_lock;

        nof_container<epoc::event_mod_notifier_user> mod_notifies;
        nof_container<epoc::event_screen_change_user> screen_changes;
        nof_container<epoc::event_error_msg_user> error_notifies;

        void create_screen_device(service::ipc_context &ctx, ws_cmd &cmd);
        void create_dsa(service::ipc_context &ctx, ws_cmd &cmd);
        void create_window_group(service::ipc_context &ctx, ws_cmd &cmd);
        void create_window_base(service::ipc_context &ctx, ws_cmd &cmd);
        void create_graphic_context(service::ipc_context &ctx, ws_cmd &cmd);
        void create_anim_dll(service::ipc_context &ctx, ws_cmd &cmd);
        void create_click_dll(service::ipc_context &ctx, ws_cmd &cmd);
        void create_sprite(service::ipc_context &ctx, ws_cmd &cmd);
        void create_wsbmp(service::ipc_context &ctx, ws_cmd &cmd);
        void get_window_group_list(service::ipc_context &ctx, ws_cmd &cmd);
        void get_number_of_window_groups(service::ipc_context &ctx, ws_cmd &cmd);
        void restore_hotkey(service::ipc_context &ctx, ws_cmd &cmd);
        void send_event_to_window_group(service::ipc_context &ctx, ws_cmd &cmd);
        void find_window_group_id(service::ipc_context &ctx, ws_cmd &cmd);
        void set_pointer_cursor_mode(service::ipc_context &ctx, ws_cmd &cmd);
        void get_window_group_client_thread_id(service::ipc_context &ctx, ws_cmd &cmd);
        void get_redraw(service::ipc_context &ctx, ws_cmd &cmd);
        void get_event(service::ipc_context &ctx, ws_cmd &cmd);
        void get_focus_window_group(service::ipc_context &ctx, ws_cmd &cmd);
        void get_window_group_name_from_id(service::ipc_context &ctx, ws_cmd &cmd);
        void clear_all_redraw_stores(service::ipc_context &ctx, ws_cmd &cmd);
        void set_window_group_ordinal_position(service::ipc_context &ctx, ws_cmd &cmd);
        void get_def_mode_max_num_colors(service::ipc_context &ctx, ws_cmd &cmd);
        void get_color_mode_list(service::ipc_context &ctx, ws_cmd &cmd);
        void set_pointer_area(service::ipc_context &ctx, ws_cmd &cmd);
        void set_pointer_cursor_position(service::ipc_context &ctx, ws_cmd &cmd);
        void get_number_of_screen(service::ipc_context &ctx, ws_cmd &cmd);

    public:
        void add_redraw_listener(notify_info nof) {
            redraws.set_listener(nof);
        }

        void add_event_listener(notify_info nof) {
            events.set_listener(nof);
        }

        void trigger_redraw() {
            redraws.trigger_notification();
        }

        epoc::version client_version() {
            return cli_version;
        }

        void execute_command(service::ipc_context &ctx, ws_cmd cmd);
        void execute_commands(service::ipc_context &ctx, std::vector<ws_cmd> cmds);
        void parse_command_buffer(service::ipc_context &ctx);

        std::uint32_t add_object(window_client_obj_ptr &obj);
        epoc::window_client_obj *get_object(const std::uint32_t handle);

        bool delete_object(const std::uint32_t handle);

        explicit window_server_client(service::session *guest_session,
            kernel::thread *own_thread, epoc::version ver);

        eka2l1::window_server &get_ws() {
            return *reinterpret_cast<window_server *>(guest_session->get_server());
        }

        kernel::thread *get_client() {
            return client_thread;
        }

        std::uint32_t queue_redraw(epoc::window_user *user);

        std::uint32_t queue_event(const event &evt) {
            return events.queue_event(evt);
        }

        void walk_event(epoc::event_fifo::walker_func walker, void *userdata) {
            events.walk(walker, userdata);
        }

        void walk_redraw(epoc::redraw_fifo::walker_func walker, void *userdata) {
            redraws.walk(walker, userdata);
        }

        void deque_redraw(const std::uint32_t handle) {
            redraws.cancel_event_queue(handle);
        }

        ws::uid add_capture_key_notifier_to_server(epoc::event_capture_key_notifier &notifier);

        // We have been blessed with so much reflection that it's actually seems evil now.
        template <typename T>
        ws::uid add_event_notifier(T &evt) {
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

    struct window_group_chain_info {
        epoc::ws::uid id;
        epoc::ws::uid parent_id;
    };

    epoc::graphics_orientation number_to_orientation(int rot);
}

namespace eka2l1 {
    static constexpr const char *WINDOW_SERVER_NAME = "!Windowserver";

    class window_server : public service::server {
    public:
        using key_capture_request_queue = cp_queue<epoc::event_capture_key_notifier>;

    private:
        friend class epoc::window_server_client;
        friend struct epoc::window_key_shipper;

        std::unordered_map<std::uint64_t, std::unique_ptr<epoc::window_server_client>>
            clients;

        common::ini_file ws_config;
        bool loaded{ false };

        std::atomic<epoc::ws::uid> key_capture_uid_counter{ 0 };
        std::atomic<epoc::ws::uid> obj_uid{ 0 };

        std::vector<epoc::config::screen> screen_configs;
        epoc::screen *screens; ///< Linked list of all screens.

        std::unordered_map<epoc::ws::uid, key_capture_request_queue> key_capture_requests;

        epoc::screen *focus_screen_{ nullptr };
        epoc::pointer_cursor_mode cursor_mode_;

        epoc::bitmap_cache bmp_cache;
        epoc::animation_scheduler anim_sched;

        fbs_server *fbss{ nullptr };
        int input_handler_evt_;

        bool key_block_active{ false };

        void init(service::ipc_context &ctx);
        void send_to_command_buffer(service::ipc_context &ctx);

        void connect(service::ipc_context &ctx) override;
        void disconnect(service::ipc_context &ctx) override;

        void on_unhandled_opcode(service::ipc_context &ctx) override;

        void load_wsini();
        void parse_wsini();

        void handle_inputs_from_driver(std::uint64_t userdata, int nn_late);
        void init_screens();

        void make_mouse_event(drivers::input_event &driver_evt_, epoc::event &guest_evt_, epoc::screen *scr);

        std::mutex input_queue_mut;
        std::queue<drivers::input_event> input_events;

    public:
        explicit window_server(system *sys);
        ~window_server();

        epoc::ws::uid next_uid() {
            return ++obj_uid;
        }

        epoc::bitmap_cache *get_bitmap_cache() {
            return &bmp_cache;
        }

        epoc::animation_scheduler *get_anim_scheduler() {
            return &anim_sched;
        }

        epoc::pointer_cursor_mode &cursor_mode() {
            return cursor_mode_;
        }

        epoc::window_group *&get_focus() {
            return focus_screen_->focus;
        }

        epoc::config::screen &get_screen_config(const int num) {
            assert(num < screen_configs.size());
            return screen_configs[num];
        }

        epoc::screen *get_screens() {
            return screens;
        }

        epoc::screen *get_current_focus_screen() {
            return focus_screen_;
        }

        void set_focus_screen(epoc::screen *scr) {
            focus_screen_ = scr;
        }

        void set_key_block_active(const bool result) {
            key_block_active = result;
        }

        fbs_server *get_fbs_server();

        epoc::screen *get_screen(const int number);

        epoc::bitwise_bitmap *get_bitmap(const std::uint32_t h);

        epoc::window_group *get_group_from_id(const epoc::ws::uid id);

        epoc::config::screen *get_current_focus_screen_config();

        /**
         * \brief Get the number of window groups running in the server
         */
        std::uint32_t get_total_window_groups(const int pri = -1, const int scr_num = 0);

        /**
         * \brief Get all window group's ID on a screen.
         */
        std::uint32_t get_window_group_list(std::uint32_t *id, const std::uint32_t max = 0, const int pri = -1, const int scr_num = 0);
        std::uint32_t get_window_group_list_and_chain(epoc::window_group_chain_info *infos, const std::uint32_t max = 0, const int pri = -1, const int scr_num = 0);

        drivers::graphics_driver *get_graphics_driver();
        ntimer *get_ntimer();
        kernel_system *get_kernel_system();

        void queue_input_from_driver(drivers::input_event &evt);
        void do_base_init();
    };
}
