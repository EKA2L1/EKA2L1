/*
 * Copyright (c) 2020 EKA2L1 Team.
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

namespace eka2l1::epoc {
    const std::uint32_t SYS_CATEGORY = 0x101f75b6;

    const std::uint32_t HAL_KEY_BASE = 0x1020e306;
    const std::uint32_t UNK_KEY1 = 0x1020e34e;

    const std::uint32_t LOCALE_DATA_KEY = 0x10208904;
    const std::uint32_t LOCALE_LANG_KEY = 0x10208903;
    const std::uint32_t LOCALE_LOCALE_SETTINGS_KEY = 0x10208905;

    const std::uint32_t PHONE_POWER_KEY = 0x100052C5;
    const std::uint32_t SOFTWARE_INSTALL_KEY = 0x102047B7;
    const std::uint32_t SOFTWARE_LASTEST_UID_INSTALLATION = 0x10272C8E;

    enum system_agent_state {
        system_agent_state_off = 0,
        system_agent_state_on = 1
    };
}