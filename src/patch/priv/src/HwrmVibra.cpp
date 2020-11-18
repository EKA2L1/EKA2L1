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

#include <HwrmVibra.hpp>
#include <HwrmConsts.hpp>

TInt RHWRMVibraRaw::Connect() {
    return iResource.Connect(EHWRMResourceTypeVibra);
}

void RHWRMVibraRaw::Close() {
    iResource.ExecuteCommand(EHWRMVibraCleanup, TIpcArgs());
    iResource.Close();
}

TInt RHWRMVibraRaw::StartVibra(TUint16 aDuration) {
    return iResource.ExecuteCommand(EHWRMVibraStartDefaultIntensity, TIpcArgs(aDuration));
}

TInt RHWRMVibraRaw::StartVibra(TUint16 aDuration, TUint16 aIntensity) {
    return iResource.ExecuteCommand(EHWRMVibraStart, TIpcArgs(aDuration, aIntensity));
}

TInt RHWRMVibraRaw::StopVibra() {
    return iResource.ExecuteCommand(EHWRMVibraStop, TIpcArgs());
}