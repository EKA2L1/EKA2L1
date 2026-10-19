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

#include <services/hwrm/haptics/protocol.h>
#include <utils/err.h>

#include <algorithm>
#include <limits>

namespace eka2l1::epoc::haptics {
    static std::int32_t read32(const std::vector<std::uint8_t> &data, std::size_t offset) {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value |= static_cast<std::uint32_t>(data[offset + i]) << (8 * i);
        }
        return static_cast<std::int32_t>(value);
    }

    static response integer(const std::int32_t value) {
        response result;
        for (int i = 0; i < 4; ++i) {
            result.data.push_back(static_cast<std::uint32_t>(value) >> (8 * i));
        }
        return result;
    }

    void device::expire(const std::uint64_t now) {
        for (auto &[handle, effect] : effects_) {
            if (effect.state == 1 && effect.duration != std::numeric_limits<std::int32_t>::max()
                && effect.elapsed + now - effect.started >= effect.duration) {
                effect.state = 0;
            }
        }
    }

    void device::stop_playing() {
        for (auto &[handle, effect] : effects_) {
            if (effect.state == 1) effect.state = 0;
        }
    }

    void device::stop_all() {
        for (auto &[handle, effect] : effects_) {
            effect.state = 0;
        }
    }

    void device::reset() {
        *this = device();
    }

    vibration device::output(const std::uint64_t now) {
        expire(now);
        for (auto it = effects_.rbegin(); it != effects_.rend(); ++it) {
            const auto &effect = it->second;
            if (effect.state != 1 || disabled_) {
                continue;
            }
            const auto remaining = effect.duration == std::numeric_limits<std::int32_t>::max()
                ? effect.duration : effect.duration - effect.elapsed - (now - effect.started);
            return {static_cast<std::uint32_t>(remaining), static_cast<std::int16_t>(effect.magnitude / 100)};
        }
        return {};
    }

    response device::execute(const std::vector<std::uint8_t> &packet, const std::uint64_t now) {
        if (packet.size() < 2) {
            return {error_argument, {}};
        }
        const auto cmd = static_cast<vibe_command>(packet[0] | (packet[1] << 8));
        const bool query = cmd == vibe_command::capability || cmd == vibe_command::get_property;
        const bool effect_command = cmd == vibe_command::pause || cmd == vibe_command::resume
            || cmd == vibe_command::stop || cmd == vibe_command::effect_state;
        const std::size_t device_offset = query ? 3 : (effect_command ? 6 : 2);
        if (packet.size() < device_offset + 4) {
            return {error_argument, {}};
        }
        const auto handle = read32(packet, device_offset);
        if (cmd == vibe_command::open) {
            // The X7 packetizer maps Any to its concrete Device actuator before encoding.
            if (packet.size() != 6 || (handle != 1 && handle != 2)) {
                return {error_not_supported, {}};
            }
            if (opened_) {
                return {error_already_exists, {}};
            }
            opened_ = true;
            return integer(1);
        }
        // Capability queries use the logical actuator, not the opened device handle.
        const bool valid_handle = cmd == vibe_command::capability ? (handle == 1 || handle == 2) : handle == 1;
        if (!opened_ || !valid_handle) {
            return {error_bad_handle, {}};
        }
        expire(now);
        switch (cmd) {
        case vibe_command::close:
        case vibe_command::stop_all:
            if (packet.size() != 6) return {error_argument, {}};
            if (cmd == vibe_command::close) reset();
            else stop_all();
            return {};

        case vibe_command::play_sweep:
        case vibe_command::play_periodic:
        case vibe_command::modify_sweep:
        case vibe_command::modify_periodic: {
            const bool modify = cmd == vibe_command::modify_sweep || cmd == vibe_command::modify_periodic;
            const bool periodic = cmd == vibe_command::play_periodic || cmd == vibe_command::modify_periodic;
            const std::size_t offset = modify ? 10 : 6;
            if (packet.size() != (modify ? 39 : 35)) return {error_argument, {}};
            const auto duration = read32(packet, offset);
            const auto magnitude = read32(packet, offset + 4);
            const auto period = read32(packet, offset + 9);
            if (duration < 0 || magnitude < 0 || magnitude > 10000
                || packet[offset + 8] != (periodic ? 1 : 0)
                || (periodic && period <= 0)) {
                return {error_argument, {}};
            }
            // The host vibrator exposes a single magnitude, rather than waveform/envelope synthesis.
            const auto attack = read32(packet, offset + 13);
            const auto attack_level = read32(packet, offset + 17);
            const auto fade = read32(packet, offset + 21);
            const auto fade_level = read32(packet, offset + 25);
            if (attack < 0 || fade < 0 || attack_level < 0 || attack_level > 10000
                || fade_level < 0 || fade_level > 10000) return {error_argument, {}};

            std::int32_t id;
            if (modify) {
                id = read32(packet, 6);
                auto it = effects_.find(id);
                if (it == effects_.end()) {
                    return {id > 0 && id < next_handle_ ? error_none : error_bad_handle, {}};
                }
                auto &value = it->second;
                if (value.state == 1) {
                    value.elapsed += now - value.started;
                    value.started = now;
                }
                value.duration = duration;
                value.magnitude = static_cast<std::uint32_t>(magnitude * static_cast<std::int64_t>(strength_) / 10000);
            } else {
                // Completed handles remain queryable until a new play needs their slot.
                for (auto it = effects_.begin(); it != effects_.end();) {
                    if (it->second.state == 0) it = effects_.erase(it);
                    else ++it;
                }
                if (effects_.size() >= 16 || next_handle_ == std::numeric_limits<std::int32_t>::max()) {
                    return {error_overflow, {}};
                }
                id = next_handle_++;
                stop_playing();
                effects_[id] = {static_cast<std::uint32_t>(duration),
                    static_cast<std::uint32_t>(magnitude * static_cast<std::int64_t>(strength_) / 10000), now, 0,
                    disabled_ ? 0 : 1};
            }
            expire(now);
            return modify ? response{} : integer(id);
        }

        case vibe_command::pause:
        case vibe_command::resume:
        case vibe_command::stop:
        case vibe_command::effect_state: {
            if (packet.size() != 10) return {error_argument, {}};
            const auto id = read32(packet, 2);
            auto it = effects_.find(id);
            if (it == effects_.end()) {
                if (id <= 0 || id >= next_handle_) return {error_bad_handle, {}};
                return cmd == vibe_command::effect_state ? integer(0) : response{};
            }
            auto &value = it->second;
            if (cmd == vibe_command::effect_state) return integer(value.state);
            if (cmd == vibe_command::stop) value.state = 0;
            else if (cmd == vibe_command::pause && value.state == 1) {
                value.elapsed += now - value.started;
                value.state = 2;
            } else if (cmd == vibe_command::resume && value.state == 2 && !disabled_) {
                stop_playing();
                value.started = now;
                value.state = 1;
            }
            return {};
        }

        case vibe_command::set_property: {
            if (packet.size() < 12) return {error_argument, {}};
            const auto property = read32(packet, 6);
            const auto type = packet[10];
            if ((type == 1 && (packet.size() != 16 || packet[11] != 4))
                || (type == 2 && (packet.size() != 13 || packet[11] != 1))) {
                return {error_argument, {}};
            }
            if (type == 2 && property == 2) {
                disabled_ = packet[12] != 0;
                if (disabled_) stop_all();
                return {};
            }
            if (type != 1) return {error_not_supported, {}};
            const auto value = read32(packet, 12);
            if (property == 1 && value >= 0 && value <= 15) priority_ = value;
            else if (property == 3 && value >= 0 && value <= 10000) strength_ = value;
            else return {error_argument, {}};
            return {};
        }

        case vibe_command::get_property: {
            if (packet.size() != 11) return {error_argument, {}};
            const auto property = read32(packet, 7);
            if (packet[2] == 2 && property == 2) return integer(disabled_ ? 1 : 0);
            if (packet[2] == 1 && property == 1) return integer(priority_);
            if (packet[2] == 1 && property == 3) return integer(strength_);
            return {error_not_supported, {}};
        }

        case vibe_command::capability: {
            if (packet.size() != 11) return {error_argument, {}};
            const auto capability = read32(packet, 7);
            if (packet[2] == 3 && capability == 10) {
                const char name[] = "EKA2L1 vibrator";
                return {error_none, std::vector<std::uint8_t>(name, name + sizeof(name) - 1)};
            }
            if (packet[2] != 1) return {error_not_supported, {}};
            switch (capability) {
            case 0: return integer(2); // Virtual device.
            case 2: case 4: case 5: return integer(1); // One actuator/slot, smooth style.
            case 3: return integer(0); // ERM actuator.
            case 6: return integer(1);
            case 7: case 8: case 11: return integer(std::numeric_limits<std::int32_t>::max());
            case 9: return integer(3); // MagSweep and Periodic; no IVT or streaming.
            default: return {error_not_supported, {}};
            }
        }
        default:
            return {error_not_supported, {}};
        }
    }
}
