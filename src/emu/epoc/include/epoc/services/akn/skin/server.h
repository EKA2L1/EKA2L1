/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <epoc/services/akn/skin/common.h>
#include <epoc/services/akn/skin/skn.h>
#include <epoc/services/akn/skin/chunk_maintainer.h>
#include <epoc/services/akn/skin/icon_cfg.h>
#include <epoc/services/akn/skin/settings.h>
#include <epoc/services/framework.h>
#include <epoc/ptr.h>

#include <queue>

#include <memory>

namespace eka2l1 {
    class akn_skin_server_session: public service::typical_session {
        eka2l1::ptr<void> client_handler_;
        epoc::notify_info nof_info_;        ///< Notify info
        std::queue<epoc::akn_skin_server_change_handler_notification> nof_list_;

        std::uint32_t flags { 0 };

        enum {
            ASS_FLAG_CANCELED = 0x1
        };

        void check_icon_config(service::ipc_context *ctx);

        void do_set_notify_handler(service::ipc_context *ctx);

        /**
         * \brief Get the next event in the notify list and notify the waiter.
         * 
         * - If cancel flag is set, the client will be notify with code KErrCancel.
         * - If the notify list is just empty, the requester of this next event will be
         *   the next one waiting.
         * - Else, the first element in the queue is popped and the client will be notify
         *   will that code.
         */
        void do_next_event(service::ipc_context *ctx);

        /**
         * \brief Cancel a pending signal request, if there is one available.
         * 
         * It will be cancel only if 2 conditions are sastified:
         * - No pending notifications.
         * - A notify handler must be set.
         */
        void do_cancel(service::ipc_context *ctx);

        void store_scaleable_gfx(service::ipc_context *ctx);

    public:
        explicit akn_skin_server_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version client_version);
        ~akn_skin_server_session() override {}

        void fetch(service::ipc_context *ctx) override;
    };

    class akn_skin_server: public service::typical_server {
        std::unique_ptr<epoc::akn_ss_settings> settings_;
        std::unique_ptr<epoc::akn_skin_icon_config_map> icon_config_map_;
        std::unique_ptr<epoc::akn_skin_chunk_maintainer> chunk_maintainer_;

        chunk_ptr skin_chunk_;
        sema_ptr skin_chunk_sema_;
        mutex_ptr skin_chunk_render_mut_;

        fbs_server *fbss;

        void do_initialisation();

        /**
         * \brief Merge active skin ID to the skin chunk.
         * \param io Pointer to the IO system.
         */
        void merge_active_skin(eka2l1::io_system *io);

    public:
        explicit akn_skin_server(eka2l1::system *sys);

        int is_icon_configured(const epoc::uid app_uid);

        void store_scaleable_gfx(
            const epoc::pid item_id,
            const epoc::skn_layout_info layout_info,
            const std::uint32_t bmp_handle,
            const std::uint32_t msk_handle
        );

        void connect(service::ipc_context &ctx) override;
    };
}
