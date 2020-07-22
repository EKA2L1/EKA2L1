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

#include <services/context.h>
#include <services/framework.h>
#include <kernel/server.h>
#include <services/ui/cap/eiksrv.h>
#include <services/ui/cap/sgc.h>
#include <services/window/window.h>

#include <mutex>

namespace eka2l1 {
    class window_server;

    namespace epoc {
        struct sgc_params {
            std::int32_t window_group_id;

            // For more information, when you encounter TBitFlags,
            // please see file: Babitflags.h in ossrv repo
            std::uint32_t bit_flags;

            std::int32_t sp_layout;
            std::int32_t sp_flag;
            std::int32_t app_screen_mode;
        };
    }

    enum oom_ui_app_op {
        akns_launch_view = 50,
        akns_kill_app = 51,
        akns_kill_all_apps = 52,
        akns_unlock_media = 53,
        akns_enable_task_list = 54,
        akns_launch_task_list = 55,
        akns_refresh_task_list = 56,
        akns_suppress_apps_key = 57,
        akns_hide_app_from_fws = 58,
        // sgc
        akn_eik_app_ui_set_sgc_params = 59,
        akn_eik_app_ui_block_server_status_pane_redraws = 60,
        akn_eik_app_ui_redraw_server_status_pane = 61,
        akn_eik_app_ui_prepare_for_app_exit = 62,
        akn_eik_app_ui_set_system_faded = 63,
        akn_eik_app_ui_is_system_faded = 64,
        akn_eik_app_ui_relinquish_priority_to_foreground_app = 65,
        akn_eik_app_ui_layout_config_size = 66,
        akn_eik_app_ui_get_layout_config = 67,
        akn_eik_app_ui_move_app_in_z_order = 68,
        // eiksrv support
        akns_set_status_pane_flags = 69,
        akns_set_status_pane_layout = 70,
        akns_blank_screen = 71,
        akns_unblank_screen = 72,
        akns_set_keyboard_repeat_rate = 73,
        akns_update_key_block_mode = 74,
        akns_show_locked_note = 75,
        akns_shutdown_apps = 76,
        akns_status_pane_resource_id = 77,
        akns_status_pane_app_resource_id = 78,
        akns_set_status_pane_app_resource_id = 79,
        akns_rotate_screen = 80,
        akns_apps_key_blocked = 81,
        akns_show_long_tap_anim = 82,
        akns_hide_long_tap_anim = 83,
        akns_get_alias_key_code = 84,
        akns_set_fg_sp_data_sub_id = 85,
        akns_cancel_shutdown_apps = 86,
        akns_get_phone_idle_view_id = 87,
        akns_pre_allocate_dynamic_soft_note_evt = 88,
        akns_notify_dynamic_soft_note_evt = 89,
        akns_cancel_dynamic_soft_note_evt_nof = 90,
        akns_discreet_popup_action = 91
    };

    // Guess the softkey is the key that displays all shortcuts app?
    enum class akn_softkey_loc {
        right,
        left,
        bottom
    };

    struct akn_screen_mode_info {
        std::int32_t mode_num;
        epoc::pixel_twips_and_rot info;
        akn_softkey_loc loc;
        std::uint32_t screen_style_hash;
        epoc::display_mode dmode;
    };

    struct akn_hardware_info {
        std::int32_t state_num;
        std::int32_t key_mode;
        std::int32_t screen_mode;
        std::int32_t alt_screen_mode;
    };

    struct akn_layout_config {
        std::int32_t num_screen_mode;
        eka2l1::ptr<akn_screen_mode_info> screen_modes;
        std::int32_t num_hardware_mode;
        eka2l1::ptr<akn_hardware_info> hardware_infos;
    };

    class oom_ui_app_session : public service::typical_session {
        std::int32_t blank_count;
        bool old_layout;

        void redraw_status_pane(service::ipc_context *ctx);

    public:
        explicit oom_ui_app_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version, const bool is_old_layout = false);
        void fetch(service::ipc_context *ctx) override;
    };

    static const char *OOM_APP_UI_SERVER_NAME = "101fdfae_10207218_AppServer";

    /*! \brief OOM App Server Members can receive notification when memory ran out and can't be
       freed. This is basically AknCapServer but loaded with this plugin.
      
      - Server type: critical.

      - Launching: HLE when not doing a full startup. A full startup should launch this server automatically.
    */
    class oom_ui_app_server : public service::typical_server {
        friend class oom_ui_app_session;

        void get_layout_config_size(service::ipc_context &ctx);
        void get_layout_config(service::ipc_context &ctx);

        std::string layout_buf;
        std::unique_ptr<epoc::cap::sgc_server> sgc;
        std::unique_ptr<epoc::cap::eik_server> eik;

        window_server *winsrv{ nullptr };

        std::mutex lock;

        void connect(service::ipc_context &ctx) override;

    protected:
        // This but except it loads the screen0 only
        void load_screen_mode();
        std::string get_layout_buf();
        void set_sgc_params(service::ipc_context &ctx, const bool old_layout);
        void update_key_block_mode(service::ipc_context &ctx);
        void redraw_status_pane();

    public:
        explicit oom_ui_app_server(eka2l1::system *sys);
        void init(kernel_system *kern);

        epoc::cap::eik_server *get_eik_server() {
            return eik.get();
        }

        epoc::cap::sgc_server *get_sgc_server();
    };
}
