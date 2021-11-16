/*
    backlightctrl.hpp
    Copyright (C) 2005 zg
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __BACKLIGHTCTRL_HPP__
#define __BACKLIGHTCTRL_HPP__

#include <e32base.h>

class MBackLightControlObserver;

class CBackLightControl: public CBase
{
  public:
    enum TBackLightType
    {
      EBackLightTypeScreen=0,
      EBackLightTypeKeys,
      EBackLightTypeBoth
    };
    enum TBackLightState
    {
      EBackLightStateOn=0,
      EBackLightStateOff,
      EBackLightStateBlink,
      EBackLightStateUnknown
    };
  public:
    virtual TInt BackLightOn(TInt aType,TUint16 aDuration)=0;
    virtual TInt BackLightBlink(TInt aType,TUint16 aDuration,TUint16 aOnTime,TUint16 aOffTime)=0;
    virtual TInt BackLightOff(TInt aType,TUint16 aDuration)=0;
    virtual TInt BackLightChange(TInt aType,TUint16 aDuration)=0;
    virtual TInt BackLightState(TInt aType)=0;
    virtual TInt SetScreenBrightness(TInt aState,TUint16 aDuration)=0;
    virtual TInt ScreenBrightness(void)=0;
  public:
    EXPORT_C static CBackLightControl* NewL(void);
    EXPORT_C static CBackLightControl* NewL(MBackLightControlObserver* aCallback);
    EXPORT_C static CBackLightControl* NewLC(MBackLightControlObserver* aCallback);
    ~CBackLightControl();
  protected:
    CBackLightControl();
};

class MBackLightControlObserver
{
  public:
    virtual void ScreenNotify(TInt aState)=0;
    virtual void KeysNotify(TInt aState)=0;
    virtual void BrightnessNotify(TInt aState)=0;
};

#endif