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

#include <control/dispatcher.h>
#include <control/params.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <exception>

namespace eka2l1::control {
    rpc_error::rpc_error(const int code, const std::string &message)
        : std::runtime_error(message)
        , code_(code) {
    }

    static std::string to_line(const rapidjson::Value &value) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);

        // The writer escapes control characters inside strings, so the output holds no raw newline.
        return std::string(buffer.GetString(), buffer.GetSize());
    }

    static void make_error(rapidjson::Value &response, const int code, const std::string &message,
        json_allocator &allocator) {
        rapidjson::Value error(rapidjson::kObjectType);
        error.AddMember("code", code, allocator);
        error.AddMember("message", rapidjson::Value(message.c_str(), static_cast<rapidjson::SizeType>(message.size()), allocator), allocator);

        response.RemoveMember("result");
        response.AddMember("error", error, allocator);
    }

    // Constant time in the length of the presented token, so its prefix cannot be guessed by timing.
    static bool tokens_match(const std::string &expected, const std::string &presented) {
        unsigned char difference = (expected.size() == presented.size()) ? 0 : 1;

        for (std::size_t i = 0; i < presented.size(); i++) {
            const char expected_char = expected.empty() ? 0 : expected[i % expected.size()];
            difference |= static_cast<unsigned char>(expected_char ^ presented[i]);
        }

        return difference == 0;
    }

    dispatcher::dispatcher() {
        add("auth", [this](session &client, const rapidjson::Value &params, rapidjson::Value &, json_allocator &) {
            const std::string token = require_string(params, "token");

            if (!token_.empty() && !tokens_match(token_, token)) {
                // One guess per connection.
                client.rejected = true;
                throw rpc_error(error_unauthorized, "The token is not the one the emulator was started with; closing the connection");
            }

            client.authenticated = true;
        });
    }

    void dispatcher::add(const std::string &name, method_handler handler) {
        methods_[name] = std::move(handler);
    }

    void dispatcher::require_token(const std::string &token) {
        token_ = token;
    }

    bool dispatcher::is_authorized(const session &client) const {
        return !client.rejected && (token_.empty() || client.authenticated);
    }

    void dispatcher::handle_call(session &client, const rapidjson::Value &call, rapidjson::Value &response,
        json_allocator &allocator) {
        response.SetObject();
        response.AddMember("jsonrpc", "2.0", allocator);

        if (!call.IsObject()) {
            response.AddMember("id", rapidjson::Value(rapidjson::kNullType), allocator);
            make_error(response, error_invalid_request, "A request must be a JSON object", allocator);
            return;
        }

        const rapidjson::Value::ConstMemberIterator id = call.FindMember("id");
        const bool has_valid_id = (id != call.MemberEnd()) && (id->value.IsNull() || id->value.IsString() || id->value.IsNumber());

        if (has_valid_id) {
            response.AddMember("id", rapidjson::Value(id->value, allocator), allocator);
        } else {
            response.AddMember("id", rapidjson::Value(rapidjson::kNullType), allocator);

            if (id != call.MemberEnd()) {
                make_error(response, error_invalid_request, "The id must be a string, a number or null", allocator);
                return;
            }
        }

        const rapidjson::Value::ConstMemberIterator version = call.FindMember("jsonrpc");
        if ((version == call.MemberEnd()) || !version->value.IsString() || (std::string_view(version->value.GetString()) != "2.0")) {
            make_error(response, error_invalid_request, "The jsonrpc member must be \"2.0\"", allocator);
            return;
        }

        const rapidjson::Value::ConstMemberIterator method = call.FindMember("method");
        if ((method == call.MemberEnd()) || !method->value.IsString()) {
            make_error(response, error_invalid_request, "The method member must be a string", allocator);
            return;
        }

        const rapidjson::Value empty_params(rapidjson::kObjectType);
        const rapidjson::Value *params = &empty_params;

        const rapidjson::Value::ConstMemberIterator params_member = call.FindMember("params");
        if (params_member != call.MemberEnd()) {
            if (!params_member->value.IsObject()) {
                make_error(response, error_invalid_params, "Parameters must be passed by name, in an object", allocator);
                return;
            }

            params = &params_member->value;
        }

        const std::string_view method_name(method->value.GetString(), method->value.GetStringLength());

        if (client.rejected) {
            make_error(response, error_unauthorized, "A wrong token was presented; the connection is closing", allocator);
            return;
        }

        if (!is_authorized(client) && (method_name != "auth")) {
            make_error(response, error_unauthorized, "Call auth with the emulator's token first", allocator);
            return;
        }

        const auto handler = methods_.find(method_name);

        if (handler == methods_.end()) {
            make_error(response, error_method_not_found, "There is no method named " + std::string(method_name), allocator);
            return;
        }

        rapidjson::Value result(rapidjson::kObjectType);

        try {
            handler->second(client, *params, result, allocator);
        } catch (rpc_error &error) {
            make_error(response, error.code(), error.what(), allocator);
            return;
        } catch (std::exception &exc) {
            make_error(response, error_internal, exc.what(), allocator);
            return;
        }

        response.AddMember("result", result, allocator);
    }

    std::string dispatcher::handle(session &client, std::string_view message) {
        // Iterative: the recursive parser's stack depth follows the nesting of the input, and a
        // line of a million '[' would overflow the server thread's stack before auth is checked.
        rapidjson::Document input;
        input.Parse<rapidjson::kParseIterativeFlag>(message.data(), message.size());

        rapidjson::Document output;
        json_allocator &allocator = output.GetAllocator();

        if (input.HasParseError()) {
            output.SetObject();
            output.AddMember("jsonrpc", "2.0", allocator);
            output.AddMember("id", rapidjson::Value(rapidjson::kNullType), allocator);
            make_error(output, error_parse, "The message is not valid JSON", allocator);

            return to_line(output);
        }

        // A notification is a valid request without an id. An invalid one is answered with id
        // null whether it has an id or not (JSON-RPC 2.0 section 7).
        const auto is_notification = [](const rapidjson::Value &call) {
            if (!call.IsObject() || call.HasMember("id")) {
                return false;
            }

            const rapidjson::Value::ConstMemberIterator version = call.FindMember("jsonrpc");
            const rapidjson::Value::ConstMemberIterator method = call.FindMember("method");

            return (version != call.MemberEnd()) && version->value.IsString()
                && (std::string_view(version->value.GetString()) == "2.0")
                && (method != call.MemberEnd()) && method->value.IsString();
        };

        if (!input.IsArray()) {
            rapidjson::Value response;
            handle_call(client, input, response, allocator);

            return is_notification(input) ? std::string() : to_line(response);
        }

        if (input.Empty()) {
            rapidjson::Value response;
            handle_call(client, rapidjson::Value(rapidjson::kNullType), response, allocator);

            response["error"]["message"].SetString("A batch must hold at least one request", allocator);
            return to_line(response);
        }

        output.SetArray();

        for (const rapidjson::Value &call : input.GetArray()) {
            rapidjson::Value response;
            handle_call(client, call, response, allocator);

            if (!is_notification(call)) {
                output.PushBack(response, allocator);
            }
        }

        return output.Empty() ? std::string() : to_line(output);
    }

    std::string make_notification(const std::string &method, const rapidjson::Value &params) {
        rapidjson::Document notification;
        json_allocator &allocator = notification.GetAllocator();

        notification.SetObject();
        notification.AddMember("jsonrpc", "2.0", allocator);
        notification.AddMember("method", rapidjson::Value(method.c_str(), static_cast<rapidjson::SizeType>(method.size()), allocator), allocator);
        notification.AddMember("params", rapidjson::Value(params, allocator), allocator);

        return to_line(notification);
    }
}
