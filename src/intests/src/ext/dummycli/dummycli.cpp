/*
 * dummycli.cpp
 *
 *  Created on: Dec 23, 2018
 *      Author: fewds
 */

#include <intests/ext/dummycli/dummycli.h>
#include <intests/ext/dummyserv/dummycmn.h>

void RDummySession::ConnectL()
    {
        TInt err = CreateSession(KDummyServName, TVersion(1, 0, 0));
        
        if (err == KErrNotFound)
            {
                // Try to summon the executable
                RProcess process;
                User::LeaveIfError(process.Create(_L("DummyServ.exe"), _L(""), EOwnerProcess));
                
                TRequestStatus status;
                
                process.Rendezvous(status);
                process.Resume();
                
                User::WaitForRequest(status);   
                process.Close();
                
                // Now create again
                User::LeaveIfError(CreateSession(KDummyServName, TVersion(1, 0, 0)));
            }
    }

// Return error code
TInt RDummySession::GetHash(TInt &aValue)
    {
        TPckg<TInt> pack(aValue);
        return SendReceive(EDummyOpHashFromNumber, TIpcArgs(&pack, 0, 0, 0));
    }

void RDummySession::GetSecertMessage(TDes &aDes)
    {
        SendReceive(EDummyOpGetSecertString, TIpcArgs(&aDes, 0, 0, 0));
    }

void RDummySession::GetSecertStruct(SSecert &aSecert)
    {
        TPckg<SSecert> pack(aSecert);
        SendReceive(EDummyOpGetSecertPackage, TIpcArgs(&pack, 0, 0, 0));
    }
