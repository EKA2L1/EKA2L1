/*
    vibractrl.hpp
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

#ifndef __VIBRACTRL_HPP__
#define __VIBRACTRL_HPP__

#include <e32base.h>

class CVibraControl: public CBase {
  public:
    enum TVibraModeState {
      EVibraModeON=0,
      EVibraModeOFF,
      EVibraModeUnknown
    };

    enum TVibraRequestStatus {
      EVibraRequestOK=0,
      EVibraRequestFail,
      EVibraRequestNotAllowed,
      EVibraRequestStopped,
      EVibraRequestUnableToStop,
      EVibraRequestUnknown
    };
    
    enum TVibraCtrlPanic {
      EPanicUnableToGetVibraSetting,
      EPanicVibraGeneral
    };
  public:
    ~CVibraControl();
    virtual void StartVibraL(TUint16 aDuration)=0;
    virtual void StopVibraL(void)=0;
    virtual TVibraModeState VibraSettings(void) const=0;
    virtual void StartVibraL(TUint16 aDuration,TInt aIntensity)=0;
  protected:
    CVibraControl();
};

class MVibraControlObserver {
  public:
    virtual void VibraModeStatus(CVibraControl::TVibraModeState aStatus)=0;
    virtual void VibraRequestStatus(CVibraControl::TVibraRequestStatus aStatus)=0;
};

class VibraFactory {
  public:
    EXPORT_C static CVibraControl* NewL(void);
    EXPORT_C static CVibraControl* NewL(MVibraControlObserver* aCallback);
    EXPORT_C static CVibraControl* NewLC(MVibraControlObserver* aCallback);
  private:
    VibraFactory();
};

#endif