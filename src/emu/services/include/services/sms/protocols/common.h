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

namespace eka2l1::epoc::sms {
    enum {
        SMS_ADDRESS_FAMILY = 0x10,
        SMS_DTG_PROTOCOL = 0x02,
        SMS_MAX_SOCKS = 0x100,
        SMS_MAX_DTG_SIZE = 255 * 160
    };

    enum ioctl {
        SMS_IOCTL_PROVIDER = 0x100,
        SMS_IOCTL_DELETE_MSG = 0x300,
        SMS_IOCTL_ENUMERATE_MSG = 0x301,
        SMS_IOCTL_READ_MSG_SUCCESS = 0x304,
        SMS_IOCTL_READ_MSG_FAILED = 0x305,
        SMS_IOCTL_SEND_SMS = 0x306,
        SMS_IOCTL_WRITE_SMS_TO_SIM = 0x307,
        SMS_IOCTL_READ_PARAMS = 0x308,
        SMS_IOCTL_READ_PARAMS_COMPLETE = 0x309,
        SMS_IOCTL_WRITE_SMS_PARAMS = 0x310,
        SMS_IOCTL_SUPPORT_OOD_CLASS0_SMS = 0x311,
        SMS_IOCTL_SELECT_MODEM_PRESENT = 0x400,
        SMS_IOCTL_SELECT_MODEM_NOT_PRESENT = 0x401,
    };
}