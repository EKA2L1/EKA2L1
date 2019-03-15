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

#include <epoc/services/context.h>
#include <epoc/services/server.h>
#include <epoc/services/window/window.h>

namespace eka2l1 {
    class window_server;

    namespace epoc {
        struct sgc_params {
            int window_group_id;

            // For more information, when you encounter TBitFlags,
            // please see file: Babitflags.h in ossrv repo
            std::uint32_t bit_flags;

            int sp_layout;
            int sp_flag;
            int app_screen_mode;
        };
    }

    enum oom_ui_app_op {
        EAknSLaunchView = 50, // to avoid collision to notifier related commands
        EAknSKillApp,
        EAknSKillAllApps,
        EAknSUnlockMedia,
        EAknSEnableTaskList,
        EAknsLaunchTaskList,
        EAknSRefreshTaskList,
        EAknSSuppressAppsKey,
        EAknSHideApplicationFromFWS,
        // sgc
        EAknEikAppUiSetSgcParams,
        EAknEikAppUiBlockServerStatusPaneRedraws,
        EAknEikAppUiRedrawServerStatusPane,
        EAknEikAppUiPrepareForAppExit,
        EAknEikAppUiSetSystemFaded,
        EAknEikAppUiIsSystemFaded,
        EAknEikAppUiRelinquishPriorityToForegroundApp,
        EAknEikAppUiLayoutConfigSize,
        EAknEikAppUiGetLayoutConfig,
        EAknEikAppUiMoveAppInZOrder,
        // eiksrv support
        EAknSSetStatusPaneFlags,
        EAknSSetStatusPaneLayout,
        EAknSBlankScreen,
        EAknSUnblankScreen,
        EAknSSetKeyboardRepeatRate,
        EAknSUpdateKeyBlockMode,
        EAknSShowLockedNote,
        EAknSShutdownApps,
        EAknSStatusPaneResourceId,
        EAknSStatusPaneAppResourceId,
        EAknSSetStatusPaneAppResourceId,
        EAknSRotateScreen,
        EAknSAppsKeyBlocked,
        EAknSShowLongTapAnimation,
        EAknSHideLongTapAnimation,
        EAknGetAliasKeyCode,
        EAknSetFgSpDataSubscriberId,
        EAknSCancelShutdownApps,
        EAknSGetPhoneIdleViewId,
        EAknSPreAllocateDynamicSoftNoteEvent,
        EAknSNotifyDynamicSoftNoteEvent,
        EAknSCancelDynamicSoftNoteEventNotification,
        EAknSDiscreetPopupAction
    };

    // Guess the softkey is the key that displays all shortcuts app?
    enum class akn_softkey_loc {
        right,
        left,
        bottom
    };

    struct akn_screen_mode_info {
        int mode_num;
        epoc::pixel_twips_and_rot info;
        akn_softkey_loc loc;
        int screen_style_hash;
        epoc::display_mode dmode;
    };

    struct akn_hardware_info {
        int state_num;
        int key_mode;
        int screen_mode;
        int alt_screen_mode;
    };

    struct akn_layout_config {
        int num_screen_mode;
        eka2l1::ptr<akn_screen_mode_info> screen_modes;
        int num_hardware_mode;
        eka2l1::ptr<akn_hardware_info> hardware_infos;
    };

    /*! \brief OOM App Server Memebers can receive notification when memory ran out and can't be
       freed. This is basiclly AknCapServer but loaded with this plugin.
      
      - Server type: critical.

      - Launching: HLE when not doing a full startup. A full startup should launch this server automaticlly.
    */
    class oom_ui_app_server : public service::server {
        void get_layout_config_size(service::ipc_context ctx);
        void get_layout_config(service::ipc_context ctx);

        std::string layout_buf;
        epoc::sgc_params params;

        window_server *winsrv {nullptr};

    protected:
        // This but except it loads the screen0 only
        void load_screen_mode();
        std::string get_layout_buf();

        void set_sgc_params(service::ipc_context ctx);

    public:
        explicit oom_ui_app_server(eka2l1::system *sys);
    };
}
