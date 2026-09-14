/*
 * Copyright (c) 2026 EKA2L1 Team.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <commdb.h>

static void Rollback(TAny* pointer) {
    static_cast<CCommsDatabase*>(pointer)->RollbackTransaction();
}

static void EnsureHostAccessPointL(CCommsDatabase& db) {
    CCommsDbTableView* iap = db.OpenTableLC(_L("IAP"));
    TInt first = iap->GotoFirstRecord();
    CleanupStack::PopAndDestroy(iap);
    if (first == KErrNone) {
        return;
    }
    User::LeaveIfError(first == KErrNotFound ? KErrNone : first);
    User::LeaveIfError(db.BeginTransaction());
    CleanupStack::PushL(TCleanupItem(Rollback, &db));
    iap = db.OpenTableLC(_L("IAP"));
    first = iap->GotoFirstRecord();
    CleanupStack::PopAndDestroy(iap);
    if (first == KErrNone) {
        db.RollbackTransaction();
        CleanupStack::Pop();
        return;
    }
    User::LeaveIfError(first == KErrNotFound ? KErrNone : first);

    TUint32 modemId = 0;
    CCommsDbTableView* modem = db.OpenViewMatchingTextLC(_L("Modem"), _L("TSYName"), _L("NTRASTSY"));
    first = modem->GotoFirstRecord();
    if (first == KErrNone) {
        modem->ReadUintL(_L("Id"), modemId);
    }
    CleanupStack::PopAndDestroy(modem);
    User::LeaveIfError(first == KErrNotFound ? KErrNone : first);
    if (!modemId) {
        modem = db.OpenTableLC(_L("Modem"));
        User::LeaveIfError(modem->InsertRecord(modemId));
        modem->WriteTextL(_L("Name"), _L("Host network modem"));
        modem->WriteTextL(_L("TSYName"), _L("NTRASTSY"));
        User::LeaveIfError(modem->PutRecordChanges());
        CleanupStack::PopAndDestroy(modem);
    }
    TUint32 locationId = 0;
    CCommsDbTableView* location = db.OpenTableLC(_L("Location"));
    first = location->GotoFirstRecord();
    if (first == KErrNone) {
        location->ReadUintL(_L("Id"), locationId);
    } else {
        User::LeaveIfError(first == KErrNotFound ? KErrNone : first);
        User::LeaveIfError(location->InsertRecord(locationId));
        location->WriteTextL(_L("Name"), _L("Host network location"));
        location->WriteBoolL(_L("Mobile"), ETrue);
        location->WriteBoolL(_L("UsePulseDial"), EFalse);
        location->WriteBoolL(_L("WaitForDialTone"), EFalse);
        location->WriteUintL(_L("PauseAfterDialout"), 0);
        User::LeaveIfError(location->PutRecordChanges());
    }
    CleanupStack::PopAndDestroy(location);
    TUint32 serviceId = 0;
    CCommsDbTableView* service = db.OpenTableLC(_L("DialOutISP"));
    User::LeaveIfError(service->InsertRecord(serviceId));
    service->WriteTextL(_L("Name"), _L("Host network"));
    service->WriteTextL(_L("IfName"), _L("ppp"));
    service->WriteTextL(_L("IfNetworks"), _L("ip"));
    service->WriteBoolL(_L("DialResolution"), EFalse);
    service->WriteBoolL(_L("UseLoginScript"), EFalse);
    service->WriteBoolL(_L("PromptForLogin"), EFalse);
    service->WriteBoolL(_L("IfPromptForAuth"), EFalse);
    service->WriteBoolL(_L("IpAddrFromServer"), ETrue);
    service->WriteBoolL(_L("IpDNSAddrFromServer"), ETrue);
    User::LeaveIfError(service->PutRecordChanges());
    CleanupStack::PopAndDestroy(service);
    TUint32 iapId = 0;
    iap = db.OpenTableLC(_L("IAP"));
    User::LeaveIfError(iap->InsertRecord(iapId));
    iap->WriteTextL(_L("Name"), _L("Host network"));
    iap->WriteTextL(_L("IAPServiceType"), _L("DialOutISP"));
    iap->WriteUintL(_L("IAPService"), serviceId);
    iap->WriteUintL(_L("Modem"), modemId);
    iap->WriteUintL(_L("Location"), locationId);
    User::LeaveIfError(iap->PutRecordChanges());
    CleanupStack::PopAndDestroy(iap);

    TUint32 wapId = 0;
    CCommsDbTableView* wap = db.OpenTableLC(_L("WAPAccessPoint"));
    User::LeaveIfError(wap->InsertRecord(wapId));
    wap->WriteTextL(_L("Name"), _L("Host network"));
    wap->WriteTextL(_L("CurrentBearer"), _L("WAPIPBearer"));
    User::LeaveIfError(wap->PutRecordChanges());
    CleanupStack::PopAndDestroy(wap);

    TUint32 bearerId = 0;
    CCommsDbTableView* bearer = db.OpenTableLC(_L("WAPIPBearer"));
    User::LeaveIfError(bearer->InsertRecord(bearerId));
    bearer->WriteUintL(_L("AccessPointId"), wapId);
    bearer->WriteUintL(_L("IAP"), iapId);
    bearer->WriteTextL(_L("GatewayAddress"), _L("0.0.0.0"));
    bearer->WriteUintL(_L("WSPOption"), 0);
    bearer->WriteBoolL(_L("Security"), EFalse);
    User::LeaveIfError(bearer->PutRecordChanges());
    CleanupStack::PopAndDestroy(bearer);

    User::LeaveIfError(db.CommitTransaction());
    CleanupStack::Pop();
}

extern "C" EXPORT_C CCommsDatabase* HostCommsDatabaseNewL(TCommDbDatabaseType type) {
    // The two-argument factory remains native and does not call the patched overload.
    TCommDbOpeningMethod openingMethod;
    CCommsDatabase* db = CCommsDatabase::NewL(type, openingMethod);
    CleanupStack::PushL(db);
    if (type == EDatabaseTypeIAP) {
        TRAP_IGNORE(EnsureHostAccessPointL(*db));
    }
    CleanupStack::Pop(db);
    return db;
}

GLDEF_C TInt E32Dll(TDllReason) {
    return KErrNone;
}
