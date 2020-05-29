/*
 * Copyright (c) 2020 EKA2L1 Team
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

namespace eka2l1::epoc::keysound {
    // I deduct from:
    // https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/uifw/AvKon/srcdata/AvkonSystemSounds.ra
    // http://www.j2megame.org/j2meapi/Nokia_UI_API_1_1/com/nokia/mid/sound/Sound.html
    // And make this table.
    // Entry starts from 0x40
    enum ring_frequency {
        ring_frequency_none = 0x40,
        ring_frequency_network_1 = 0x4E,

        ring_frequency_a0 = 0x56,
        ring_frequency_b0b = 0x57,
        ring_frequency_b0 = 0x58,
        ring_frequency_c0 = 0x59,
        ring_frequency_d0b = 0x5A,
        ring_frequency_d0 = 0x5B,
        ring_frequency_e0b = 0x5C,
        ring_frequency_e0 = 0x5D,
        ring_frequency_f0 = 0x5E,
        ring_frequency_g0b = 0x5F,
        ring_frequency_g0 = 0x60,

        ring_frequency_a1b = 0x61,
        ring_frequency_a1 = 0x62,
        ring_frequency_b1b = 0x63,
        ring_frequency_b1 = 0x64,
        ring_frequency_c1 = 0x65,
        ring_frequency_d1b = 0x66,
        ring_frequency_d1 = 0x67,
        ring_frequency_e1b = 0x68,
        ring_frequency_e1 = 0x69,
        ring_frequency_f1 = 0x6A,
        ring_frequency_g1b = 0x6B,
        ring_frequency_g1 = 0x6C,

        ring_frequency_a2b = 0x6D,
        ring_frequency_a2 = 0x6E,
        ring_frequency_b2b = 0x6F,
        ring_frequency_b2 = 0x70,
        ring_frequency_c2 = 0x71,
        ring_frequency_d2b = 0x72,
        ring_frequency_d2 = 0x73,
        ring_frequency_e2b = 0x74,
        ring_frequency_e2 = 0x75,
        ring_frequency_f2 = 0x76,
        ring_frequency_g2b = 0x77,
        ring_frequency_g2 = 0x78,
        ring_frequency_a3b = 0x79,
        ring_frequency_a3 = 0x7A
    };

    static std::map<ring_frequency, std::int32_t> frequency_map{
        { ring_frequency_network_1, 425 }, { ring_frequency_a0, 220 },
        { ring_frequency_b0b, 233 }, { ring_frequency_b0, 247 },
        { ring_frequency_c0, 262 }, { ring_frequency_d0b, 277 },
        { ring_frequency_d0, 294 }, { ring_frequency_e0b, 311 },
        { ring_frequency_e0, 330 }, { ring_frequency_f0, 349 },
        { ring_frequency_g0b, 370 }, { ring_frequency_g0, 392 },
        { ring_frequency_a1b, 416 }, { ring_frequency_a1, 440 },
        { ring_frequency_b1b, 466 }, { ring_frequency_b1, 494 },
        { ring_frequency_c1, 523 }, { ring_frequency_d1b, 554 },
        { ring_frequency_d1, 587 }, { ring_frequency_e1b, 622 },
        { ring_frequency_e1, 659 }, { ring_frequency_f1, 698 },
        { ring_frequency_a2b, 831 }, { ring_frequency_a2, 880 },
        { ring_frequency_b2b, 932 }, { ring_frequency_b2, 988 },
        { ring_frequency_c2, 1047 }, { ring_frequency_d2b, 1109 },
        { ring_frequency_d2, 1175 }, { ring_frequency_e2b, 1245 },
        { ring_frequency_e2, 1319 }, { ring_frequency_f2, 1397 },
        { ring_frequency_g2b, 1480 }, { ring_frequency_g2, 1568 },
        { ring_frequency_a3b, 1661 }, { ring_frequency_a3, 1760 }
    };
}