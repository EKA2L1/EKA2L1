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
#include <control/keys.h>
#include <control/params.h>

#include <drivers/input/common.h>
#include <services/window/window.h>
#include <system/epoc.h>

#include <chrono>
#include <thread>

namespace eka2l1::control {
    // How long "tap" holds a key or a touch down unless told otherwise.
    static constexpr std::int64_t DEFAULT_HOLD_MS = 50;
    static constexpr std::int64_t MAX_HOLD_MS = 10000;
    static constexpr std::int64_t MAX_POINTERS = 8;

    enum class input_action {
        press,
        move,
        release,
        tap
    };

    static input_action read_action(const rapidjson::Value &params, const bool allow_move) {
        const std::string action = optional_string(params, "action").value_or("tap");

        if (action == "press") {
            return input_action::press;
        }

        if (action == "release") {
            return input_action::release;
        }

        if (action == "tap") {
            return input_action::tap;
        }

        if (allow_move && (action == "move")) {
            return input_action::move;
        }

        throw rpc_error(error_invalid_params, allow_move ? "Parameter 'action' must be \"press\", \"move\", \"release\" or \"tap\"" : "Parameter 'action' must be \"press\", \"release\" or \"tap\"");
    }

    static void deliver(context &ctx, drivers::input_event evt) {
        ctx.run_in_guest([&](system &sys) {
            require_window_server(sys).queue_input_from_driver(evt);
        });
    }

    // Press, let the guest run for `hold`, release.
    static void deliver_tap(context &ctx, drivers::input_event press, drivers::input_event release,
        const std::chrono::milliseconds hold) {
        deliver(ctx, press);

        const auto until = std::chrono::steady_clock::now() + hold;
        while (!ctx.stopping && (std::chrono::steady_clock::now() < until)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        deliver(ctx, release);
    }

    static std::uint32_t read_scan_code(const rapidjson::Value &params) {
        const std::optional<std::string> name = optional_string(params, "key");
        const std::optional<std::int64_t> scan_code = optional_integer(params, "scancode", 0, 0xFFFF);

        if (name.has_value() == scan_code.has_value()) {
            throw rpc_error(error_invalid_params, "Give either 'key' (a key name) or 'scancode' (a standard scan code)");
        }

        if (scan_code) {
            return static_cast<std::uint32_t>(*scan_code);
        }

        const std::optional<std::uint32_t> named = scan_code_of_key(*name);

        if (!named) {
            throw rpc_error(error_invalid_params, "Unknown key '" + *name + "'; README.md lists the key names");
        }

        return *named;
    }

    void add_input_methods(dispatcher &rpc, context &ctx) {
        rpc.add("input.key", [&ctx](session &, const rapidjson::Value &params, rapidjson::Value &, json_allocator &) {
            const std::uint32_t scan_code = read_scan_code(params);
            const input_action action = read_action(params, false);
            const std::chrono::milliseconds hold(optional_integer(params, "hold_ms", 0, MAX_HOLD_MS).value_or(DEFAULT_HOLD_MS));

            // Raw scan codes: delivered as the guest would see them, whatever the key bindings.
            drivers::input_event evt;
            evt.type_ = drivers::input_event_type::key_raw;
            evt.key_.code_ = static_cast<int>(scan_code);
            evt.key_.state_ = (action == input_action::release) ? drivers::key_state::released : drivers::key_state::pressed;

            if (action != input_action::tap) {
                deliver(ctx, evt);
                return;
            }

            drivers::input_event release = evt;
            release.key_.state_ = drivers::key_state::released;

            deliver_tap(ctx, evt, release, hold);
        });

        rpc.add("input.touch", [&ctx](session &, const rapidjson::Value &params, rapidjson::Value &, json_allocator &) {
            const int x = static_cast<int>(require_integer(params, "x", 0, 0xFFFF));
            const int y = static_cast<int>(require_integer(params, "y", 0, 0xFFFF));
            const input_action action = read_action(params, true);
            const std::uint32_t pointer = static_cast<std::uint32_t>(optional_integer(params, "pointer", 0, MAX_POINTERS - 1).value_or(0));
            const std::chrono::milliseconds hold(optional_integer(params, "hold_ms", 0, MAX_HOLD_MS).value_or(DEFAULT_HOLD_MS));

            // Positions are guest screen pixels, not host window pixels.
            drivers::input_event evt;
            evt.type_ = drivers::input_event_type::touch;
            evt.mouse_.raw_screen_pos_ = true;
            evt.mouse_.pos_x_ = x;
            evt.mouse_.pos_y_ = y;
            evt.mouse_.pos_z_ = 0;
            evt.mouse_.button_ = drivers::mouse_button_left;
            evt.mouse_.mouse_id = pointer;

            switch (action) {
            case input_action::press:
            case input_action::tap:
                evt.mouse_.action_ = drivers::mouse_action_press;
                break;

            case input_action::move:
                evt.mouse_.action_ = drivers::mouse_action_repeat;
                break;

            case input_action::release:
                evt.mouse_.action_ = drivers::mouse_action_release;
                break;
            }

            if (action != input_action::tap) {
                deliver(ctx, evt);
                return;
            }

            drivers::input_event release = evt;
            release.mouse_.action_ = drivers::mouse_action_release;

            deliver_tap(ctx, evt, release, hold);
        });
    }
}
