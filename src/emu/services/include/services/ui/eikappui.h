/*
 * Copyright (c) 2018 EKA2L1 Team
 * Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include <cstdint>
#include <services/framework.h>

namespace eka2l1 {
    class oom_ui_app_server;
    class oom_ui_app_session;

    enum eik_app_ui_ranges {
        eik_app_ui_range_no_cap = 0,
        eik_app_ui_range_sgc = 13
    };

    enum eik_app_ui_opcode {
        // No Capability requirement
        eik_app_ui_first = eik_app_ui_range_no_cap,
        eik_app_ui_launch_task_list,
        eik_app_ui_cycle_tasks,
        eik_app_ui_set_status_pane_flags,
        eik_app_ui_set_status_pane_layout,
        eik_app_ui_blank_screen,
        eik_app_ui_unblank_screen,
        eik_app_ui_resolve_error,
        eik_app_ui_extension,
        eik_app_ui_enable_task_list,
        eik_app_ui_debug_prefs,
        eik_app_ui_resolve_error_with_title_text,
        eik_app_ui_unused
    };

    struct debug_preferences {
        int flags{ 0 };
        int spare{ 0 };

        std::string to_buf();
    };

    class eikappui_session: public service::typical_session {
    private:
        oom_ui_app_session *cap_session_;

    public:
        explicit eikappui_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version client_version);
        ~eikappui_session() override;

        void fetch(service::ipc_context *ctx) override;
    };

    // Unless app do weird stuff like checking if the eik app ui semaphore exists,
    // this server will does thing just fine
    class eikappui_server : public service::typical_server {
    protected:
        friend class eikappui_session;

        oom_ui_app_server *cap_server_;

        void get_debug_preferences(service::ipc_context &ctx);

        void connect(service::ipc_context &ctx) override;
        void disconnect(service::ipc_context &ctx) override;

    public:
        explicit eikappui_server(eka2l1::system *sys);
    };
}
