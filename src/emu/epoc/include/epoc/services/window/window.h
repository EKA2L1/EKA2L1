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
#include <unordered_map>
#include <vector>
#include <queue>

#include <common/ini.h>
#include <common/queue.h>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

#include <epoc/ptr.h>
#include <epoc/services/server.h>
#include <epoc/utils/des.h>

enum {
    cmd_slot = 0,
    reply_slot = 1,
    remote_slot = 2
};

constexpr int twips_mul = 15;

namespace eka2l1 {
    class window_server;

    struct ws_cmd_header {
        uint16_t op;
        uint16_t cmd_len;
    };

    struct ws_cmd {
        ws_cmd_header header;
        uint32_t obj_handle;

        void *data_ptr;
    };
}

namespace eka2l1::epoc {
    namespace config {
        struct screen_mode {
            int screen_number;
            int mode_number;

            eka2l1::vec2 size;
            int rotation;
        };
        
        struct screen {
            int screen_number;
            std::vector<screen_mode> modes;
        };
    }

    struct window;
    using window_ptr = std::shared_ptr<epoc::window>;

    enum class window_kind {
        normal,
        group,
        top_client,
        client
    };

    class window_server_client;
    using window_server_client_ptr = window_server_client *;

    struct screen_device;
    using screen_device_ptr = std::shared_ptr<epoc::screen_device>;

    enum class graphics_orientation { 
        normal,
        rotated90, 
        rotated180,
        rotated270 
    };

    /*! \brief Screen display mode.
     *
     * Depend on the display mode, the bitmap sends it will have the specified attribute.
    */
    enum class display_mode {
        none,
        gray2,          ///< Monochrome display mode (1 bpp)
        gray4,          ///< Four grayscales display mode (2 bpp)
        gray16,         ///< 16 grayscales display mode (4 bpp) 
        gray256,        ///< 256 grayscales display mode (8 bpp) 
        color16,        ///< Low colour EGA 16 colour display mode (4 bpp) 
        color256,       ///< 256 colour display mode (8 bpp) 
        color64k,       ///< 64,000 colour display mode (16 bpp) 
        color16m,       ///< True colour display mode (24 bpp) 
        rgb,            ///< (Not an actual display mode used for moving buffers containing bitmaps)
        color4k,        ///< 4096 colour display (12 bpp). 
        color16mu,      ///< True colour display mode (32 bpp, but top byte is unused and unspecified) 
        color16ma,      ///< Display mode with alpha (24bpp colour plus 8bpp alpha) 
        color16map,     ///< Pre-multiplied Alpha display mode (24bpp color multiplied with the alpha channel value, plus 8bpp alpha)
        color_last
    };

    enum class pointer_cursor_mode {
        none,           ///< The device don't have a pointer (touch)
        fixed,          ///< Use the default system cursor
        normal,         ///< Can't understand yet
        window,         ///< Can't understand yet
    };

    enum class window_type {
        redraw,
        backed_up,
        blank
    };

    enum class event_modifier {
        repeatable = 0x001,
        keypad = 0x002,
        left_alt = 0x004,
        right_alt = 0x008,
        alt = 0x010,
        left_ctrl = 0x020,
        right_ctrl = 0x040,
        ctrl = 0x080,
        left_shift = 0x100,
        right_shift = 0x200,
        shift = 0x400,
        left_func = 0x800,
        right_func = 0x1000,
        func = 0x2000,
        caps_lock = 0x4000,
        num_lock = 0x8000,
        scroll_lock = 0x10000,
        key_up = 0x20000,
        special = 0x40000,
        double_click = 0x80000,
        modifier_pure_key_code = 0x100000,
        cancel_rot = 0x200000,
        no_rot = 0x0,
        rotate90 = 0x400000,
        rotate180 = 0x800000,
        rotate270 = 0x1000000,
        all_mods = 0x1FFFFFFF                   
    };

    enum class text_aligment {
        left,
        center,
        right
    };

    enum class event_control {
        always,
        only_with_keyboard_focus,
        only_when_visible
    };

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

    enum class event_type {
        button1down,
        button1up,
        button2down,
        button2up,
        drag,
        move,
        button_repeat,
        repeat,
        switch_on,
        out_of_range
    };

    struct pointer_event {
        event_type evtype;
        event_modifier modifier;
        eka2l1::vec2 pos;
        eka2l1::vec2 parent_pos;
    };

    struct adv_pointer_event: public pointer_event {
        int spare1;
        int spare2;
        int pos_z;
        std::uint8_t ptr_num; // multi touch
    };

    enum pointer_filter_type {
        pointer_enter = 0x01,   ///< In/out
        pointer_move = 0x02,
        pointer_drag = 0x04,
        pointer_simulated_event = 0x08,
        all = pointer_move | pointer_simulated_event
    };

    struct event {
        int type;
        std::uint32_t handle;
        std::uint64_t time;

        // TODO: Should be only pointer event with epoc < 9.
        // For epoc9 there shouldnt be a pointer number, since there is no multi touch
        adv_pointer_event evt;
    };

    struct redraw_event {
        std::uint32_t handle;
        vec2 top_left;
        vec2 bottom_right;
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

    struct window_client_obj {
        uint32_t id;
        window_server_client *client;

        explicit window_client_obj(window_server_client_ptr client);
        virtual ~window_client_obj() {}

        virtual void execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd cmd);
    };

    using window_client_obj_ptr = std::shared_ptr<window_client_obj>;

    /*! \brief Base class for all window. */
    struct window : public window_client_obj {
        eka2l1::cp_queue<window_ptr> childs;
        screen_device_ptr dvc;

        window_ptr parent;

        // It's just z value. The second one will be used when there is 
        // multiple window with same first z.
        std::uint16_t priority { 0 };
        std::uint16_t secondary_priority { 0 };

        // The position
        eka2l1::vec2  pos { 0, 0 };
        eka2l1::vec2  size { 0, 0 };

        window_kind type;

        bool operator==(const window &rhs) {
            return priority == rhs.priority;
        }

        bool operator!=(const window &rhs) {
            return priority != rhs.priority;
        }

        bool operator>(const window &rhs) {
            return priority > rhs.priority;
        }

        bool operator<(const window &rhs) {
            return priority < rhs.priority;
        }

        bool operator>=(const window &rhs) {
            return priority >= rhs.priority;
        }

        bool operator<=(const window &rhs) {
            return priority <= rhs.priority;
        }

        bool execute_command_for_general_node(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd cmd);

        window(window_server_client_ptr client)
            : window_client_obj(client)
            , type(window_kind::normal)
            , dvc(nullptr) {}

        window(window_server_client_ptr client, window_kind type)
            : window_client_obj(client)
            , type(type)
            , dvc(nullptr) {}

        window(window_server_client_ptr client, screen_device_ptr dvc, window_kind type)
            : window_client_obj(client)
            , type(type)
            , dvc(dvc) {}
    };

    struct window_group;
    using window_group_ptr = std::shared_ptr<epoc::window_group>;

    struct screen_device : public window_client_obj {
        eka2l1::graphics_driver_client_ptr driver;
        int screen;

        epoc::config::screen scr_config;
        epoc::config::screen_mode   *crr_mode;

        std::vector<epoc::window_ptr> windows;
        epoc::window_group_ptr  focus;

        screen_device(window_server_client_ptr client, int number, 
            eka2l1::graphics_driver_client_ptr driver);

        void execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd cmd) override;
    };

    struct window_group : public epoc::window {
        epoc::window_group_ptr next_sibling { nullptr };
        std::u16string name;

        bool accept_keyfocus { false };

        window_group(window_server_client_ptr client, screen_device_ptr dvc)
            : window(client, dvc, window_kind::group) {
        }

        void execute_command(service::ipc_context &context, ws_cmd cmd) override;
    };

    struct graphic_context;

    struct window_user : public epoc::window {
        epoc::display_mode dmode;
        epoc::window_type win_type;

        std::vector<epoc::graphic_context*> contexts;

        std::uint32_t clear_color = 0xFFFFFFFF;
        std::uint32_t filter = pointer_filter_type::all;

        eka2l1::vec2 cursor_pos { -1, -1 };

        bool allow_pointer_grab;

        struct invalidate_rect {
            vec2 in_top_left;
            vec2 in_bottom_right;
        } irect;

        window_user (window_server_client_ptr client, screen_device_ptr dvc,
            epoc::window_type type_of_window, epoc::display_mode dmode)
            : window(client, dvc, window_kind::client), win_type(type_of_window),
              dmode(dmode)
        {

        }
        
        int shadow_height { 0 };
        bool shadow_disable = false;
        
        bool activate = false;

        void execute_command(service::ipc_context &context, ws_cmd cmd) override;
    };

    struct draw_command {
        int gc_command;
        std::string buf;

        template <typename T>
        T internalize() {
            T d = *reinterpret_cast<T*>(&(buf[0]));
            buf.erase(0, sizeof(T));

            return d;
        }

        template <typename T>
        void externalize(const T &v) {
            buf.append(reinterpret_cast<const char*>(&v), sizeof(T));
        }
    };

    struct graphic_context : public window_client_obj {
        std::shared_ptr<window_user> attached_window;
        std::queue<draw_command> draw_queue;

        bool recording { false };

        void flush_queue_to_driver();

        void do_command_draw_text(service::ipc_context &ctx, eka2l1::vec2 top_left, eka2l1::vec2 bottom_right, std::u16string text);

        void active(service::ipc_context &context, ws_cmd cmd);
        void execute_command(service::ipc_context &context, ws_cmd cmd) override;

        explicit graphic_context(window_server_client_ptr client, screen_device_ptr scr = nullptr,
            window_ptr win = nullptr);
    };

    // Is this a 2D game engine ?
    struct sprite : public window_client_obj {
        window_ptr attached_window;
        eka2l1::vec2 position;

        void execute_command(service::ipc_context &context, ws_cmd cmd) override;
        explicit sprite(window_server_client_ptr client, window_ptr attached_window = nullptr,
            eka2l1::vec2 pos = eka2l1::vec2(0, 0));
    };

    struct anim_dll: public window_client_obj {
        // Nothing yet
        anim_dll(window_server_client_ptr client)
            : window_client_obj(client) {

        }

        std::uint32_t user_count {0};

        void execute_command(service::ipc_context &context, ws_cmd cmd) override;
    };

    struct click_dll : public window_client_obj {
        click_dll(window_server_client_ptr client)
            : window_client_obj(client) {

        }

        bool loaded { false };

        void execute_command(service::ipc_context &context, ws_cmd cmd) override;
    };

    class window_server_client {
        friend struct window_client_obj;

        session_ptr guest_session;

        std::vector<window_client_obj_ptr> objects;
        std::vector<epoc::screen_device_ptr> devices;

        epoc::screen_device_ptr primary_device;
        epoc::window_ptr root;

        eka2l1::thread_ptr client_thread;
        eka2l1::epoc::window_group_ptr last_group;

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

    public:
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
    };
    
    epoc::graphics_orientation number_to_orientation(int rot);
}

namespace eka2l1 {
    struct ws_cmd_screen_device_header {
        int num_screen;
        uint32_t screen_dvc_ptr;
    };

    struct ws_cmd_window_header {
        int parent;
        uint32_t client_handle;

        epoc::window_type win_type;
        epoc::display_mode dmode;
    };

    struct ws_cmd_window_group_header {
        uint32_t client_handle;
        bool focus;
        uint32_t parent_id;
        uint32_t screen_device_handle;
    };

    struct ws_cmd_create_sprite_header {
        int window_handle;
        eka2l1::vec2 base_pos;
        int flags;
    };

    struct ws_cmd_ordinal_pos_pri {
        int pri2;
        int pri1;
    };

    struct ws_cmd_find_window_group_identifier {
        std::uint32_t parent_identifier;
        int offset;
        int length;
    };

    struct ws_cmd_set_extent {
        eka2l1::vec2 pos;
        eka2l1::vec2 size;
    };

    struct ws_cmd_pointer_filter {
        std::uint32_t mask;
        std::uint32_t flags;
    };

    struct ws_cmd_draw_text_ptr {
        eka2l1::vec2 pos;
        eka2l1::ptr<epoc::desc16> text;
    };

    struct ws_cmd_draw_box_text_ptr {
        vec2 left_top_pos;
        vec2 right_bottom_pos;
        int baseline_offset;
        epoc::text_aligment horiz;
        int left_mgr;
        int width;
        eka2l1::ptr<epoc::desc16> text;
    };

    struct ws_cmd_set_text_cursor {
        uint32_t win;
        vec2 pos;

        // TODO: Add more
    };

    struct ws_cmd_send_event_to_window_group {
        int id;
        epoc::event evt;
    };
    
    struct ws_cmd_draw_text_vertical_v94 {
        vec2 pos;
        vec2 bottom_right;
        int unk1;
        int length;
    };

    struct ws_cmd_get_window_group_name_from_id {
        std::uint32_t id;
        int max_len;
    };
}

namespace eka2l1 {
    struct event_notify_info {
        eka2l1::ptr<epoc::request_status> sts;
        eka2l1::thread_ptr requester;
        eka2l1::epoc::window_server_client *client;
    };

    struct event_wrapper {
        epoc::event evt;
        epoc::window_server_client *client;
    };

    struct redraw_event_wrapper {
        epoc::redraw_event evt;
        epoc::window_server_client *client;
    };

    class window_server : public service::server {
        std::unordered_map<std::uint64_t, std::shared_ptr<epoc::window_server_client>>
            clients;

        common::ini_file    ws_config;
        bool                loaded { false };

        std::vector<epoc::config::screen>   screens;

        epoc::window_group_ptr focus_;
        epoc::pointer_cursor_mode   cursor_mode_;

        std::vector<event_wrapper> pending_events;
        std::vector<redraw_event_wrapper> pending_redraws;

        std::vector<event_notify_info> statuses;
        std::vector<event_notify_info> redraw_statuses;

        void init(service::ipc_context ctx);
        void send_to_command_buffer(service::ipc_context ctx);

        void on_unhandled_opcode(service::ipc_context ctx) override;

        void load_wsini();
        void parse_wsini();

    public:
        window_server(system *sys);

        std::optional<epoc::redraw_event> get_redraw(epoc::window_server_client *cli);
        std::optional<epoc::event> get_event(epoc::window_server_client *cli);

        void queue_event(epoc::window_server_client *cli, epoc::event evt);
        void queue_redraw_start(epoc::window_server_client *cli, epoc::redraw_event evt);

        void add_to_notify(event_notify_info info);
        void add_to_redraw_notify(event_notify_info info);

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