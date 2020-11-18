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

#include <HwrmBase.hpp>
#include <HwrmConsts.hpp>

#include <Log.h>

TVersion RHWRMResourceClient::ServerVersion() {
    return TVersion(KHWRMServerVerMajor, KHWRMServerVerMinor, KHWRMServerVerBuild);
}

TInt RHWRMResourceClient::Connect(THWRMResourceType aType) {
    TInt err = CreateSession(KHWRMServerName, ServerVersion(), KHWRMDefaultAsyncSlots);

    if (err != KErrNone) {
        LogOut(KHWRMLogCategory, _L("Error while trying to connect to HWRMServer!"));
        return err;
    }

    switch (aType) {
    case EHWRMResourceTypeVibra:
        err = SendReceive(EHWRMServiceFactoryCreateVibra, TIpcArgs());
        break;

    case EHWRMResourceTypeLight:
        err = SendReceive(EHWRMServiceFactoryCreateLight, TIpcArgs());
        break;

    default:
        err = KErrNotSupported;
        break;
    }

    if (err != KErrNone) {
        LogOut(KHWRMLogCategory, _L("Error while trying to instantiate a HWRM service (type %d)!"), 
            static_cast<TInt>(err));

        return err;
    }

    return KErrNone;
}

TInt RHWRMResourceClient::ExecuteCommand(const TInt aCommand, const TIpcArgs &aArgs) {
    TInt res = SendReceive(aCommand, aArgs);
    return res;
}

void RHWRMResourceClient::ExecuteAsyncCommand(const TInt aCommand, const TIpcArgs &aArgs, TRequestStatus &aStatus) {
    SendReceive(aCommand, aArgs, aStatus);
}