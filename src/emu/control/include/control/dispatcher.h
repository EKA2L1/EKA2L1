/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <rapidjson/document.h>

#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace eka2l1::control {
    /**
     * \brief JSON-RPC 2.0 error codes.
     *
     * The first five are defined by JSON-RPC 2.0; the rest use the range it reserves for
     * the server. README.md says when each one is returned.
     */
    enum error_code : int {
        error_parse = -32700,
        error_invalid_request = -32600,
        error_method_not_found = -32601,
        error_invalid_params = -32602,
        error_internal = -32603,
        error_not_ready = -32000,
        error_unauthorized = -32001,
        error_not_found = -32002,
        error_failed = -32003,
        error_shutting_down = -32004
    };

    /**
     * \brief Thrown by a method handler to answer the request with a JSON-RPC error.
     */
    class rpc_error : public std::runtime_error {
        int code_;

    public:
        explicit rpc_error(const int code, const std::string &message);

        int code() const {
            return code_;
        }
    };

    /**
     * \brief State the server keeps for one client connection.
     */
    struct session {
        bool authenticated = false;

        // Set by a failed auth. Nothing else on the connection runs, not even the rest of its
        // batch, and the server closes it once the answer is written.
        bool rejected = false;

        std::set<std::string> subscriptions;
    };

    using json_allocator = rapidjson::Document::AllocatorType;

    /**
     * \brief Handles one method call.
     *
     * \param client    The connection the call came from.
     * \param params    The call's named parameters; an empty object when the call has none.
     * \param result    An empty object on entry; the handler fills it in.
     * \param allocator Allocator for the values put into `result`.
     *
     * Throws rpc_error to answer with an error instead.
     */
    using method_handler = std::function<void(session &client, const rapidjson::Value &params,
        rapidjson::Value &result, json_allocator &allocator)>;

    /**
     * \brief Turns JSON-RPC 2.0 messages into method calls and their responses.
     *
     * One message is one line of text: a request, a notification or a batch of them.
     * The dispatcher itself is not thread-safe; the server calls it from one thread.
     */
    class dispatcher {
        std::map<std::string, method_handler, std::less<>> methods_;
        std::string token_;

        void handle_call(session &client, const rapidjson::Value &call, rapidjson::Value &response,
            json_allocator &allocator);

    public:
        explicit dispatcher();

        /**
         * \brief Register a method. A later registration under the same name replaces it.
         */
        void add(const std::string &name, method_handler handler);

        /**
         * \brief Require each connection to call `auth` with this token before anything else.
         *
         * An empty token turns the check off, which is the default.
         */
        void require_token(const std::string &token);

        /**
         * \brief Whether the connection may call methods: it passed auth, or no token is required.
         */
        bool is_authorized(const session &client) const;

        /**
         * \brief Handle one message.
         *
         * \returns The response line to send back, without its newline. Empty when the
         *          message held only notifications, which are never answered.
         */
        std::string handle(session &client, std::string_view message);
    };

    /**
     * \brief Make a server-to-client notification (a request without an id) as one line.
     */
    std::string make_notification(const std::string &method, const rapidjson::Value &params);
}
