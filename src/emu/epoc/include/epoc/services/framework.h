/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
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

#include <epoc/services/server.h>
#include <epoc/services/context.h>
#include <epoc/utils/obj.h>

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>

namespace eka2l1::service {
    using uid = std::uint32_t;

    class normal_object_container: public epoc::object_container {        
        using ref_count_object_heap_ptr = std::unique_ptr<epoc::ref_count_object>;
        std::vector<ref_count_object_heap_ptr> objs;

        std::atomic<uid> uid_counter {1};

    public:
        template <typename T>
        T *get(const service::uid id) {
            auto result = std::lower_bound(objs.begin(), objs.end(), id, 
                [](const ref_count_object_heap_ptr &lhs, const service::uid &rhs) {
                    return lhs->id < rhs;
                });

            if (result == objs.end()) {
                return nullptr;
            }

            return reinterpret_cast<T*>((*result).get());
        }

        template <typename T, typename ...Args>
        T *make_new(Args... arguments) {
            ref_count_object_heap_ptr obj = std::make_unique<T>(arguments...);
            obj->id = uid_counter++;

            objs.push_back(std::move(obj));

            return reinterpret_cast<T*>(objs.back().get());
        }

        bool remove(epoc::ref_count_object *obj) override;
    };

    class typical_session;
    using typical_session_ptr = std::unique_ptr<typical_session>;

    class typical_server: public server {
        friend class typical_session;
        std::unordered_map<service::uid, typical_session_ptr> sessions;

    protected:
        normal_object_container obj_con;

    public:
        template <typename T>
        T *get(const service::uid handle) {
            return obj_con.get<T>(handle);
        }

        template <typename T, typename ...Args>
        T *create_session(service::ipc_context *ctx, Args... arguments) {
            const service::uid suid = ctx->msg->msg_session->unique_id();
            sessions.emplace(suid, std::make_unique<T>(
                reinterpret_cast<typical_server*>(this), suid, arguments...));

            return reinterpret_cast<T*>(sessions[suid].get());
        }

        template <typename T, typename ...Args>
        T *make_new(Args... arguments) {
            return obj_con.make_new<T, Args...>(arguments...);
        }

        explicit typical_server(system *sys, const std::string name);
        void process_accepted_msg() override;

        void disconnect(service::ipc_context &ctx) override;
    };
    
    class typical_session {
        typical_server *svr_;

    protected:
        service::uid client_ss_uid_;
        epoc::object_table obj_table_;

    public:
        explicit typical_session(typical_server *svr, service::uid client_ss_uid)
            : svr_(svr), client_ss_uid_(client_ss_uid) {
        }

        template <typename T, typename ...Args>
        T *make_new(Args... arguments) {
            return svr_->make_new<T, Args...>(arguments...);
        }

        template <typename T>
        T *server() {
            return reinterpret_cast<T*>(svr_);
        }

        virtual void fetch(service::ipc_context *ctx) = 0;
    };
}
