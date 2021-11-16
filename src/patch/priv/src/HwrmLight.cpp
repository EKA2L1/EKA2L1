/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <HwrmConsts.hpp>
#include <HwrmLight.hpp>

TInt RHWRMLightRaw::Connect() {
    return iResource.Connect(EHWRMResourceTypeLight);
}

void RHWRMLightRaw::Close() {
    iResource.ExecuteCommand(EHWRMLightCleanup, TIpcArgs());
    iResource.Close();
}

TInt RHWRMLightRaw::LightOn(THWRMLightsOnData &data) {
    TPckgC<THWRMLightsOnData> dataPckg(data);
    return iResource.ExecuteCommand(EHWRMLightOn, TIpcArgs(&dataPckg));
}

TInt RHWRMLightRaw::LightOff(THWRMLightsOffData &data) {
    TPckgC<THWRMLightsOffData> dataPckg(data);
    return iResource.ExecuteCommand(EHWRMLightOff, TIpcArgs(&dataPckg));
}

TInt RHWRMLightRaw::LightBlink(THWRMLightsBlinkData &data) {
    TPckgC<THWRMLightsBlinkData> dataPckg(data);
    return iResource.ExecuteCommand(EHWRMLightBlink, TIpcArgs(&dataPckg));
}