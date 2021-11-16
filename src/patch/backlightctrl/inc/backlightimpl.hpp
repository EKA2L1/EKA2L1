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

#include <HwrmLight.hpp>
#include <coemain.h>

#include "backlightctrl.hpp"

class CBackLightControlImpl: public CBackLightControl, public MCoeForegroundObserver {
public:
    CBackLightControlImpl(MBackLightControlObserver* aCallback);
    ~CBackLightControlImpl();

    EXPORT_C TInt BackLightOn(TInt aType,TUint16 aDuration);
    EXPORT_C TInt BackLightBlink(TInt aType,TUint16 aDuration,TUint16 aOnTime,TUint16 aOffTime);
    EXPORT_C TInt BackLightOff(TInt aType,TUint16 aDuration);
    EXPORT_C TInt BackLightChange(TInt aType,TUint16 aDuration);
    EXPORT_C TInt BackLightState(TInt aType);
    EXPORT_C TInt SetScreenBrightness(TInt aState,TUint16 aDuration);
    EXPORT_C TInt ScreenBrightness();

    void ConstructL();

public:
    void HandleGainingForeground();
    void HandleLosingForeground();

private:
    void Close();
    void Open();

    MBackLightControlObserver *iObserver;
    RHWRMLightRaw iController;

    TBool iCaptured;
};