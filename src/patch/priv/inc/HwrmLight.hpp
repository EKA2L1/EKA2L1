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

#ifndef __HWRM_LIGHT_HPP__
#define __HWRM_LIGHT_HPP__

#include <HwrmBase.hpp>

enum THWRMLightSimpleTarget {
    EHWRMLightSimpleTargetDisplay = 1 << 0,
    EHWRMLightSimpleTargetKeyboard = 1 << 1
};

struct THWRMLightsOnData {
    TInt iTarget;
    TInt iIntensity;
    TInt iDuration;
    TBool iFadeIn;
    TUint iColor;
};

struct THWRMLightsOffData {
    TInt iTarget;
    TInt iDuration;
    TInt iFadeOut;
};

struct THWRMLightsBlinkData {
    TInt iTarget;
    TInt iIntensity;
    TInt iDuration;
    TInt iOnCycleDuration;
    TInt iOffCycleDuration;
    TUint iColor;
};

class RHWRMLightRaw {
private:
    RHWRMResourceClient iResource;

public:
    TInt Connect();
    void Close();

    TInt LightOn(THWRMLightsOnData &data);
    TInt LightOff(THWRMLightsOffData &data);
    TInt LightBlink(THWRMLightsBlinkData &data);
};

#endif