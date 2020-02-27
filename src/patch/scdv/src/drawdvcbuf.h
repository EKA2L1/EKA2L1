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

#include <scdv/draw.h>
#include <scdv/log.h>

class CFbsDrawDeviceBuffer: public CFbsDrawDevice {
protected:
    TAny *iBuffer;
    TPoint iDitherOrigin;
    TUint8 iBlackMap;
    TUint8 iWhiteMap;
    
public:
    virtual TInt InitScreen();
    virtual void SetBits(TAny* aBits);
    
    virtual void SetAutoUpdate(TBool aValue);
    
    virtual TInt SetCustomPalette(const CPalette*);
    virtual TInt GetCustomPalette(CPalette*&);
    virtual void SetDisplayMode(CFbsDrawDevice* aDrawDevice);
    virtual void SetDitherOrigin(const TPoint&);
    virtual void SetUserDisplayMode(TDisplayMode);
    virtual void SetShadowMode(TShadowMode);
    virtual void SetFadingParameters(TUint8 /*aBlackMap*/,TUint8 /*aWhiteMap*/);
    virtual void ShadowArea(const TRect&);
    virtual void ShadowBuffer(TInt,TUint32*);
    virtual void Update();
    virtual void Update(const TRegion&);
    virtual void UpdateRegion(const TRect&);
};
