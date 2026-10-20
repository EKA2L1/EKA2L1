/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace eka2l1::epoc::haptics {
    enum opcode {
        command = 2000,
        create_service,
        play_effect,
        supported_actuators,
        open_actuator,
        set_license,
        reserve,
        release,
        cleanup,
        get_status,
        status_notification,
        bridge_command
    };

    enum class vibe_command : std::uint16_t {
        stop = 7, stop_all = 8, capability = 9,
        set_property = 16, get_property = 17, open = 18, close = 19,
        pause = 25, resume = 26, effect_state = 27,
        play_sweep = 28, play_periodic = 29, modify_sweep = 30, modify_periodic = 31
    };

    struct response {
        int error = 0;
        std::vector<std::uint8_t> data;
    };

    struct vibration {
        std::uint32_t duration = 0;
        std::int16_t intensity = 0;
        bool operator==(const vibration &other) const {
            return duration == other.duration && intensity == other.intensity;
        }
    };

    // Vibe packet fields are packed, including unaligned 32-bit values after a 16-bit opcode.
    class device {
        struct effect {
            std::uint32_t duration = 0;
            std::uint32_t magnitude = 0;
            std::uint64_t started = 0;
            std::uint64_t elapsed = 0;
            int state = 1;
        };

        bool opened_ = false;
        bool disabled_ = false;
        std::int32_t priority_ = 0;
        std::int32_t strength_ = 10000;
        std::int32_t next_handle_ = 1;
        std::map<std::int32_t, effect> effects_;
        void expire(std::uint64_t now);
        void stop_playing();

    public:
        response execute(const std::vector<std::uint8_t> &packet, std::uint64_t now);
        vibration output(std::uint64_t now);
        void stop_all();
        void reset();
        bool opened() const { return opened_; }
        std::int32_t priority() const { return priority_; }
    };
}
