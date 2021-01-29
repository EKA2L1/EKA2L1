/*
 * Copyright (c) 2021 EKA2L1 Team
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

namespace eka2l1::epoc::bt {
    enum {
        BTADDR_PROTOCOL_FAMILY_ID = 0x0101,
        BTLINK_MANAGER_PROTOCOL_ID = 0x0099,
        L2CAP_PROTOCOL_ID = 0x0103,
        SDP_PROTOCOL_ID = 0x0001,
        RFCOMM_PROTOCOL_ID = 0x0003,

        SOL_BT_BLOG = 0x1000,
        SOL_BT_HCI = 0x1010,
        SOL_BT_LINK_MANAGER = 0x1011,
        SOL_BT_L2CAP = 0x1012,
        SOL_BT_RFCOMM = 0x1013
    };
}