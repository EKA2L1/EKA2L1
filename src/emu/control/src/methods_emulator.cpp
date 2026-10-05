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
#include <control/frontend.h>
#include <control/params.h>

#include <common/types.h>
#include <common/version.h>
#include <system/devices.h>
#include <system/epoc.h>

#include <mutex>

namespace eka2l1::control {
    // Bumped when a method changes in a way an existing client would notice.
    static constexpr int PROTOCOL_VERSION = 1;

    static void describe_device(system *sys, rapidjson::Value &result, json_allocator &allocator) {
        rapidjson::Value device(rapidjson::kNullType);
        device_manager *devices = sys ? sys->get_device_manager() : nullptr;

        if (devices) {
            const std::lock_guard<std::mutex> guard(devices->lock);

            if (const eka2l1::device *current = devices->get_current()) {
                device.SetObject();
                device.AddMember("manufacturer", rapidjson::Value(current->manufacturer.c_str(), allocator), allocator);
                device.AddMember("model", rapidjson::Value(current->model.c_str(), allocator), allocator);
                device.AddMember("firmware", rapidjson::Value(current->firmware_code.c_str(), allocator), allocator);
                device.AddMember("os", rapidjson::StringRef(epocver_to_string(current->ver)), allocator);
            }
        }

        result.AddMember("device", device, allocator);
    }

    void add_emulator_methods(dispatcher &rpc, context &ctx) {
        rpc.add("emulator.info", [&ctx](session &, const rapidjson::Value &, rapidjson::Value &result, json_allocator &allocator) {
            const std::string version = std::string(GIT_BRANCH) + "-" + GIT_COMMIT_HASH;

            result.AddMember("name", "EKA2L1", allocator);
            result.AddMember("version", rapidjson::Value(version.c_str(), allocator), allocator);
            result.AddMember("protocol", PROTOCOL_VERSION, allocator);
            result.AddMember("paused", ctx.host.is_paused(), allocator);

            describe_device(ctx.host.get_system(), result, allocator);
        });

        rpc.add("emulator.pause", [&ctx](session &, const rapidjson::Value &, rapidjson::Value &result, json_allocator &allocator) {
            ctx.host.pause();

            // Wait out the slice in flight, so no guest instruction runs after the answer.
            if (ctx.host.get_system()) {
                ctx.run_in_guest([](system &) {});
            }

            result.AddMember("paused", true, allocator);
        });

        rpc.add("emulator.resume", [&ctx](session &, const rapidjson::Value &, rapidjson::Value &result, json_allocator &allocator) {
            ctx.host.resume();
            result.AddMember("paused", false, allocator);
        });

        rpc.add("emulator.exit", [&ctx](session &, const rapidjson::Value &params, rapidjson::Value &, json_allocator &) {
            const int code = static_cast<int>(optional_integer(params, "code", 0, 255).value_or(0));
            ctx.host.request_exit(code);
        });
    }
}
