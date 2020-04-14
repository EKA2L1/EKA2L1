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

#include "drawdvc24.h"
#include "drawdvc32.h"
#include <scdv/draw.h>
#include <scdv/panic.h>

#include <hal.h>

CFbsDrawDevice *CFbsDrawDevice::NewBitmapDeviceL(const TSize &aSize, TDisplayMode aDispMode, TInt aDataStride) {
    CFbsDrawDevice *newDevice = NULL;

    switch (aDispMode) {
    case EColor16M:
        newDevice = new (ELeave) CFbsTwentyfourBitDrawDevice;
        CleanupStack::PushL(newDevice);
        User::LeaveIfError(reinterpret_cast<CFbsTwentyfourBitDrawDevice *>(newDevice)->Construct(aSize, aDataStride));

        Scdv::Log("INFO:: A new 24 bit bitmap device has been instantiated!");

        break;

    case EColor16MA:
        newDevice = new (ELeave) CFbsTwentyfourBitAlphaDrawDevice;
        CleanupStack::PushL(newDevice);
        User::LeaveIfError(reinterpret_cast<CFbsTwentyfourBitAlphaDrawDevice *>(newDevice)->Construct(aSize, aDataStride));

        Scdv::Log("INFO:: A new 24 bit alpha bitmap device has been instantiated!");

        break;

    case EColor16MU:
        newDevice = new (ELeave) CFbsThirtyTwoBitsDrawDevice;
        CleanupStack::PushL(newDevice);
        User::LeaveIfError(reinterpret_cast<CFbsThirtyTwoBitsDrawDevice *>(newDevice)->Construct(aSize, aDataStride));

        Scdv::Log("INFO:: A new 32 bit bitmap device has been instantiated!");

        break;

    default:
        Scdv::Log("ERR:: Unsupported or unimplemented format for bitmap device %d", aDispMode);
        Scdv::Panic(Scdv::EPanicUnsupported);
    }

    CleanupStack::Pop(newDevice);
    return newDevice;
}

CFbsDrawDevice *CFbsDrawDevice::NewBitmapDeviceL(TScreenInfo aInfo, TDisplayMode aDispMode, TInt aDataStride) {
    return NewBitmapDeviceL(aInfo.iScreenSize, aDispMode, aDataStride);
}

static CFbsDrawDevice *InstantiateNewScreenDevice(const TUint32 aScreenNo, TAny *aAddress, const TSize aSize, const TDisplayMode aMode) {
    CFbsDrawDevice *device = NULL;

    switch (aMode) {
    case EColor16MA:
        device = new (ELeave) CFbsTwentyfourBitAlphaScreenDrawDevice;
        CleanupStack::PushL(device);
        User::LeaveIfError(reinterpret_cast<CFbsTwentyfourBitAlphaScreenDrawDevice *>(device)->Construct(aScreenNo, aSize, -1));

        Scdv::Log("INFO:: A new 24 bit alpha screen device has been instantiated!");

        break;

    default:
        Scdv::Log("ERROR:: Unsupported display mode for screen device %d", (TInt)aMode);
        Scdv::Panic(Scdv::EPanicUnsupported);

        return NULL;
    }

    if (!device) {
        Scdv::Panic(Scdv::EPanicLogFailure);
        return NULL;
    }

    device->SetBits(aAddress);
    CleanupStack::Pop(device);
    return device;
}

CFbsDrawDevice *CFbsDrawDevice::NewScreenDeviceL(TScreenInfo aInfo, TDisplayMode aDispMode) {
    return InstantiateNewScreenDevice(0, aInfo.iScreenAddress, aInfo.iScreenSize, aDispMode);
}

CFbsDrawDevice *CFbsDrawDevice::NewScreenDeviceL(TInt aScreenNo, TDisplayMode aDispMode) {
    TUint32 *videoAddress = NULL;
    HAL::Get(aScreenNo, HAL::EDisplayMemoryAddress, (TInt &)(videoAddress));

    TInt width, height = 0;
    HAL::Get(aScreenNo, HAL::EDisplayXPixels, width);
    HAL::Get(aScreenNo, HAL::EDisplayYPixels, height);

    return InstantiateNewScreenDevice(aScreenNo, videoAddress, TSize(width, height), aDispMode);
}
