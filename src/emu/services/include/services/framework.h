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

#include <services/context.h>
#include <kernel/kernel.h>
#include <kernel/server.h>
#include <kernel/session.h>
#include <utils/obj.h>
#include <utils/version.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace eka2l1::service {
    using uid = std::uint32_t;

    class normal_object_container : public epoc::object_container {
        using ref_count_object_heap_ptr = std::unique_ptr<epoc::ref_count_object>;
        std::vector<ref_count_object_heap_ptr> objs;
        std::mutex obj_lock;
        std::atomic<uid> uid_counter{ 1 };

    public:
        ~normal_object_container() override;
        void clear() override;

        template <typename T>
        T *get(const service::uid id) {
            const std::lock_guard<std::mutex> guard(obj_lock);

            auto result = std::lower_bound(objs.begin(), objs.end(), id,
                [](const ref_count_object_heap_ptr &lhs, const service::uid &rhs) {
                    return lhs->id < rhs;
                });

            if (result == objs.end()) {
                return nullptr;
            }

            return reinterpret_cast<T *>((*result).get());
        }

        template <typename T, typename... Args>
        T *make_new(Args... arguments) {
            ref_count_object_heap_ptr obj = std::make_unique<T>(arguments...);
            obj->id = uid_counter++;
            obj->owner = this;

            const std::lock_guard<std::mutex> guard(obj_lock);
            objs.push_back(std::move(obj));

            return reinterpret_cast<T *>(objs.back().get());
        }

        decltype(objs)::iterator begin() {
            return objs.begin();
        }

        decltype(objs)::iterator end() {
            return objs.end();
        }

        bool remove(epoc::ref_count_object *obj) override;
    };

    class typical_session;
    using typical_session_ptr = std::unique_ptr<typical_session>;

    class typical_server : public server {
    protected:
        friend class typical_session;

        normal_object_container obj_con;
        std::unordered_map<kernel::uid, typical_session_ptr> sessions;

        std::optional<epoc::version> get_version(service::ipc_context *ctx);

    public:
        ~typical_server() override;

        void clear_all_sessions() {
            sessions.clear();
        }

        template <typename T>
        T *get(const service::uid handle) {
            return obj_con.get<T>(handle);
        }

        template <typename T>
        T *session(const kernel::uid session_uid) {
            if (sessions.find(session_uid) == sessions.end()) {
                return nullptr;
            }

            return reinterpret_cast<T *>(&*sessions[session_uid]);
        }

        template <typename T, typename... Args>
        T *create_session_impl(const kernel::uid suid, const epoc::version client_version, Args... arguments) {
            sessions.emplace(suid, std::make_unique<T>(reinterpret_cast<typical_server *>(this), suid, client_version, arguments...));

            auto &target_session = sessions[suid];
            return reinterpret_cast<T *>(target_session.get());
        }

        template <typename T, typename... Args>
        T *create_session(service::ipc_context *ctx, Args... arguments) {
            const kernel::uid suid = ctx->msg->msg_session->unique_id();
            std::optional<epoc::version> client_version = get_version(ctx);

            if (!client_version) {
                return nullptr;
            }

            return create_session_impl<T>(suid, client_version.value(), arguments...);
        }

        bool remove_session(const kernel::uid sid) {
            const auto ite = sessions.find(sid);

            if (ite == sessions.end()) {
                return false;
            }

            sessions.erase(ite);
            return true;
        }

        template <typename T, typename... Args>
        T *make_new(Args... arguments) {
            return obj_con.make_new<T, Args...>(arguments...);
        }

        template <typename T>
        bool remove(T *obj) {
            return obj_con.remove(reinterpret_cast<epoc::ref_count_object *>(obj));
        }

        explicit typical_server(system *sys, const std::string &name);
        void process_accepted_msg() override;

        void disconnect(service::ipc_context &ctx) override;

        void destroy() override {
            clear_all_sessions();
            server::destroy();
        }
    };

    class typical_session {
        typical_server *svr_;

    protected:
        kernel::uid client_ss_uid_;
        epoc::object_table obj_table_;
        epoc::version ver_;

    public:
        explicit typical_session(typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver)
            : svr_(svr)
            , client_ss_uid_(client_ss_uid)
            , ver_(client_ver) {
        }

        virtual ~typical_session() {}

        template <typename T, typename... Args>
        T *make_new(Args... arguments) {
            return svr_->make_new<T, Args...>(arguments...);
        }

        template <typename T>
        T *server() {
            return reinterpret_cast<T *>(svr_);
        }

        epoc::version &client_version() {
            return ver_;
        }

        virtual void fetch(service::ipc_context *ctx) = 0;
    };
}
