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

#pragma once

#include <epoc/utils/reqsts.h>

#include <epoc/services/framework.h>
#include <epoc/services/cdl/common.h>
#include <epoc/services/cdl/observer.h>

#include <memory>

namespace eka2l1 {
    class cdl_server_session: public service::typical_session {
        epoc::notify_info notifier;
        std::string temp_buf;

        void do_get_refs_size(service::ipc_context *ctx);
        void do_get_temp_buf(service::ipc_context *ctx);
        void do_get_plugin_drive(service::ipc_context *ctx);

    public:
        explicit cdl_server_session(service::typical_server *svr, service::uid client_ss_uid);
        void fetch(service::ipc_context *ctx) override;
    };

    /**
     * \brief Custom data layout server.
     * 
     * This server provides access to custom layouts for apps and games.
     * 
     * Imagine your main screen on the phone. You can have 3 type of layouts for its: Completely 
     * no widgets, menu button at the middle of screen or, contact button and menu button at 
     * bottom of the scrreen. These layout are not hardcoded, but rather stores in a folder
     * with a plugin making them layout to work and layout reference for the phone to draw.
     * 
     * That's how CDL server does the job. It scans layout files in specific folder in each
     * drive, and then provide them to client when it's asked to.
     * 
     * \see akn_icon_server akn_skin_server
     */
    class cdl_server: public service::typical_server {
        friend class cdl_server_session;

        epoc::cdl_ref_collection collection_;
        
        std::unique_ptr<epoc::cdl_ecom_generic_observer> observer_;
        std::unique_ptr<epoc::cdl_ecom_watcher> watcher_;

        void init();

    public:
        explicit cdl_server(eka2l1::system *sys);

        void connect(service::ipc_context ctx) override;

        void add_refs(epoc::cdl_ref_collection &col_);
        void remove_refs(const std::u16string &name);
    };
}
