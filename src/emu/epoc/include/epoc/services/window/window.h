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

#include <common/ini.h>
#include <common/queue.h>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

#include <epoc/ptr.h>
#include <epoc/services/server.h>

enum {
    cmd_slot = 0,
    reply_slot = 1
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

        virtual void execute_command(eka2l1::service::ipc_context ctx, eka2l1::ws_cmd cmd) {
        }
    };

    using window_client_obj_ptr = std::shared_ptr<window_client_obj>;

    /*! \brief Base class for all window. */
    struct window : public window_client_obj {
        eka2l1::cp_queue<window_ptr> childs;
        screen_device_ptr dvc;

        window_ptr parent;
        uint16_t priority;

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

        void execute_command(eka2l1::service::ipc_context ctx, eka2l1::ws_cmd cmd) override;
    };

    struct window_group : public epoc::window {
        window_group(window_server_client_ptr client, screen_device_ptr dvc)
            : window(client, dvc, window_kind::group) {
        }

        eka2l1::vec2 get_screen_size() const {
            return dvc->driver->screen_size();
        }

        void adjust_screen_size(const eka2l1::object_size scr_size) const {
        }
    };

    struct window_user : public epoc::window {
        epoc::display_mode dmode;
        epoc::window_type win_type;

        window_user (window_server_client_ptr client, screen_device_ptr dvc,
            epoc::window_type type_of_window, epoc::display_mode dmode)
            : window(client, dvc, window_kind::client), win_type(type_of_window),
              dmode(dmode)
        {

        }
    };

    struct graphic_context : public window_client_obj {
        window_ptr attached_window;

        void active(service::ipc_context context, ws_cmd cmd);
        void execute_command(service::ipc_context context, ws_cmd cmd) override;

        explicit graphic_context(window_server_client_ptr client, screen_device_ptr scr = nullptr,
            window_ptr win = nullptr);
    };

    // Is this a 2D game engine ?
    struct sprite : public window_client_obj {
        window_ptr attached_window;
        eka2l1::vec2 position;

        void execute_command(service::ipc_context context, ws_cmd cmd) override;
        explicit sprite(window_server_client_ptr client, window_ptr attached_window = nullptr,
            eka2l1::vec2 pos = eka2l1::vec2(0, 0));
    };

    class window_server_client {
        friend struct window_client_obj;

        session_ptr guest_session;

        std::vector<window_client_obj_ptr> objects;
        std::vector<epoc::screen_device_ptr> devices;

        epoc::screen_device_ptr primary_device;
        epoc::window_ptr root;

        void create_screen_device(service::ipc_context ctx, ws_cmd cmd);
        void create_window_group(service::ipc_context ctx, ws_cmd cmd);
        void create_window_base(service::ipc_context ctx, ws_cmd cmd);
        void create_graphic_context(service::ipc_context ctx, ws_cmd cmd);
        void create_sprite(service::ipc_context ctx, ws_cmd cmd);

        void restore_hotkey(service::ipc_context ctx, ws_cmd cmd);

        void init_device(epoc::window_ptr &win);
        epoc::window_ptr find_window_obj(epoc::window_ptr &root, std::uint32_t id);

    public:
        void execute_command(service::ipc_context ctx, ws_cmd cmd);
        void execute_commands(service::ipc_context ctx, std::vector<ws_cmd> cmds);
        void parse_command_buffer(service::ipc_context ctx);

        std::uint32_t add_object(window_client_obj_ptr obj);
        window_client_obj_ptr get_object(const std::uint32_t handle);

        bool delete_object(const std::uint32_t handle);

        explicit window_server_client(session_ptr guest_session);

        eka2l1::window_server &get_ws() {
            return *(std::reinterpret_pointer_cast<window_server>(guest_session->get_server()));
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
}

namespace eka2l1 {
    class window_server : public service::server {
        std::unordered_map<std::uint64_t, std::shared_ptr<epoc::window_server_client>>
            clients;

        common::ini_file    ws_config;
        bool                loaded { false };

        std::vector<epoc::config::screen>   screens;

        epoc::window_group_ptr focus_;
        epoc::pointer_cursor_mode   cursor_mode_;

        void init(service::ipc_context ctx);
        void send_to_command_buffer(service::ipc_context ctx);

        void on_unhandled_opcode(service::ipc_context ctx) override;

        void load_wsini();
        void parse_wsini();

    public:
        window_server(system *sys);

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