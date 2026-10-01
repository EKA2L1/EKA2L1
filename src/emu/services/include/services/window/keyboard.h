/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#pragma once

#include <services/window/common.h>
#include <optional>

namespace eka2l1::epoc {
    class key_event_translator {
        std::uint32_t modifiers_ = 0;

    public:
        std::uint32_t modifiers() const {
            return modifiers_;
        }

        static bool is_shift(std::uint32_t scan) {
            return scan == std_key_left_shift || scan == std_key_right_shift;
        }

        static bool is_repeatable(std::uint32_t scan) {
            return !is_shift(scan) && scan != std_key_device_0 && scan != std_key_device_1;
        }

        std::optional<event> translate(event &raw) {
            const auto scan = static_cast<std_scan_code>(raw.key_evt_.scancode);
            if (is_shift(scan)) {
                const auto bit = scan == std_key_left_shift ? event_modifier_left_shift : event_modifier_right_shift;
                if (raw.type == event_code::key_down) {
                    modifiers_ |= bit;
                } else if (raw.type == event_code::key_up) {
                    modifiers_ &= ~bit;
                }
                modifiers_ &= ~event_modifier_shift;
                if (modifiers_ & (event_modifier_left_shift | event_modifier_right_shift)) {
                    modifiers_ |= event_modifier_shift;
                }
            }

            raw.key_evt_.modifiers = modifiers_;
            // WSERV reports modifier transitions through key up/down, never EEventKey.
            if (is_shift(scan) || raw.type != event_code::key_down) {
                return std::nullopt;
            }

            event key = raw;
            key.type = event_code::key;
            key.key_evt_.code = map_scancode_to_keycode(scan);
            if (is_repeatable(scan)) {
                key.key_evt_.modifiers |= event_modifier_repeatable;
            }
            return key;
        }
    };
}
