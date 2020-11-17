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

#ifndef __HWRM_CONSTS_HPP__
#define __HWRM_CONSTS_HPP__

#include <e32std.h>

_LIT(KHWRMServerName, "!HWRMServer");
_LIT(KHWRMLogCategory, "HWRMCli");

const TUint32 KHWRMServerVerMajor = 1;
const TUint32 KHWRMServerVerMinor = 1;
const TUint32 KHWRMServerVerBuild = 1;
const TUint32 KHWRMDefaultAsyncSlots = 12;

enum THWRMServiceFactoryOpcode {
    EHWRMServiceFactoryCreateVibra = 0,
    EHWRMServiceFactoryCreateLight = 1
};

enum THWRMVibraServiceOpcode {
    EHWRMVibraStartDefaultIntensity = 2000,
    EHWRMVibraStart = 2001,
    EHWRMVibraStop = 2002,
    EHWRMVibraCleanup = 2003,
    EHWRMVibraRelease = 2004
};

#endif