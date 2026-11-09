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

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eka2l1::drivers {
    /**
     * @brief Check if the data starts like a Nokia Smart Messaging ringing tone (OTA bitmap).
     *
     * J2ME games of the time (com.nokia.mid.sound.Sound, FORMAT_TONE) hand these to the
     * phone's audio player, which no host decoder understands.
     */
    bool is_nokia_ringtone(const std::uint8_t *data, const std::size_t size);

    /**
     * @brief Convert a Nokia Smart Messaging ringing tone to a Standard MIDI File.
     *
     * @param data   The ringing tone.
     * @param size   Its size in bytes.
     * @param result The MIDI file, filled on success.
     *
     * @returns False if the data is not a ringing tone this converter can read.
     */
    bool nokia_ringtone_to_midi(const std::uint8_t *data, const std::size_t size, std::vector<std::uint8_t> &result);
}
