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
#include <scdv/panic.h>
#include "drawdvc24.h"

CFbsDrawDevice* CFbsDrawDevice::NewBitmapDeviceL(const TSize& aSize, TDisplayMode aDispMode, TInt aDataStride) {
	CFbsDrawDevice *newDevice = NULL;

	switch (aDispMode) {
		case EColor16M:
			newDevice = new (ELeave) CFbsTwentyfourBitDrawDevice;
			CleanupStack::PushL(newDevice);
			User::LeaveIfError(reinterpret_cast<CFbsTwentyfourBitDrawDevice*>(newDevice)->Construct(aSize, aDataStride));
			break;
		
		default:
			Scdv::Log("ERR:: Unsupported or unimplemented format for bitmap device %d", aDispMode);
			Scdv::Panic(Scdv::EPanicUnsupported);
	}
	
	CleanupStack::Pop(newDevice);
	return newDevice;
}

CFbsDrawDevice* CFbsDrawDevice::NewBitmapDeviceL(TScreenInfo aInfo, TDisplayMode aDispMode, TInt aDataStride) {
	return NewBitmapDeviceL(aInfo.iScreenSize, aDispMode, aDataStride);
}

CFbsDrawDevice* CFbsDrawDevice::NewScreenDeviceL(TScreenInfo aInfo, TDisplayMode aDispMode) {
	Scdv::Log("ERR:: Unsupported creating screen device, TODO!");
	Scdv::Panic(Scdv::EPanicUnsupported);
	return NULL;
}

CFbsDrawDevice* CFbsDrawDevice::NewScreenDeviceL(TInt aScreenNo, TDisplayMode aDispMode) {
	Scdv::Log("ERR:: Unsupported creating screen device, TODO!");
	Scdv::Panic(Scdv::EPanicUnsupported);
	return NULL;
}
