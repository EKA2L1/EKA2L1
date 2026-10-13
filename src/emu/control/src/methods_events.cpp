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

#include "context.h"

#include <control/dispatcher.h>

#include <array>
#include <string>
#include <vector>

namespace eka2l1::control {
    // Each event `x` arrives as the notification `event.x`.
    static constexpr std::array<const char *, 1> EVENTS = { "app_exited" };

    static std::vector<std::string> read_events(const rapidjson::Value &params) {
        const rapidjson::Value::ConstMemberIterator member = params.FindMember("events");

        if ((member == params.MemberEnd()) || !member->value.IsArray()) {
            throw rpc_error(error_invalid_params, "Parameter 'events' must be an array of event names");
        }

        std::vector<std::string> names;

        for (const rapidjson::Value &name : member->value.GetArray()) {
            if (!name.IsString()) {
                throw rpc_error(error_invalid_params, "Parameter 'events' must be an array of event names");
            }

            const std::string event(name.GetString(), name.GetStringLength());
            bool known = false;

            for (const char *candidate : EVENTS) {
                known = known || (event == candidate);
            }

            if (!known) {
                throw rpc_error(error_invalid_params, "There is no event named '" + event + "'");
            }

            names.push_back(event);
        }

        return names;
    }

    static void list_subscriptions(const session &client, rapidjson::Value &result, json_allocator &allocator) {
        rapidjson::Value events(rapidjson::kArrayType);

        for (const std::string &event : client.subscriptions) {
            events.PushBack(rapidjson::Value(event.c_str(), allocator), allocator);
        }

        result.AddMember("events", events, allocator);
    }

    void add_event_methods(dispatcher &rpc, context &ctx) {
        rpc.add("events.subscribe", [&ctx](session &client, const rapidjson::Value &params, rapidjson::Value &result, json_allocator &allocator) {
            const std::vector<std::string> events = read_events(params);

            ctx.run_in_guest([&](system &sys) {
                ctx.hook_app_exits(sys);
            });

            client.subscriptions.insert(events.begin(), events.end());
            list_subscriptions(client, result, allocator);
        });

        rpc.add("events.unsubscribe", [](session &client, const rapidjson::Value &params, rapidjson::Value &result, json_allocator &allocator) {
            for (const std::string &event : read_events(params)) {
                client.subscriptions.erase(event);
            }

            list_subscriptions(client, result, allocator);
        });
    }
}
