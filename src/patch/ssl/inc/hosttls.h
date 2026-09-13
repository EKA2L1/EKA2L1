// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EKA2L1_HOST_TLS_H
#define EKA2L1_HOST_TLS_H
#include <securesocketinterface.h>
#include "genericsocket.h"

class CSocketTransfer;
class CHostTls : public CBase, public MSecureSocket {
public:
    IMPORT_C static MSecureSocket* NewL(RSocket&, const TDesC&);
    IMPORT_C static void UnloadDll(TAny*);
    IMPORT_C static MSecureSocket* NewL(MGenericSecureSocket&, const TDesC&);
    ~CHostTls();
    TInt AvailableCipherSuites(TDes8&);
    void CancelAll();
    void CancelHandshake();
    void CancelRecv();
    void CancelSend();
    const CX509Certificate* ClientCert();
    TClientCertMode ClientCertMode();
    void Close();
    TInt CurrentCipherSuite(TDes8&);
    TDialogMode DialogMode();
    void FlushSessionCache();
    TInt GetOpt(TUint, TUint, TDes8&);
    TInt GetOpt(TUint, TUint, TInt&);
    TInt Protocol(TDes&);
    void Recv(TDes8&, TRequestStatus&);
    void RecvOneOrMore(TDes8&, TRequestStatus&, TSockXfrLength&);
    void RenegotiateHandshake(TRequestStatus&);
    void Send(const TDesC8&, TRequestStatus&);
    void Send(const TDesC8&, TRequestStatus&, TSockXfrLength&);
    const CX509Certificate* ServerCert();
    TInt SetAvailableCipherSuites(const TDesC8&);
    TInt SetClientCert(const CX509Certificate&);
    TInt SetClientCertMode(TClientCertMode);
    TInt SetDialogMode(TDialogMode);
    TInt SetOpt(TUint, TUint, const TDesC8&);
    TInt SetOpt(TUint, TUint, TInt);
    TInt SetProtocol(const TDesC&);
    TInt SetServerCert(const CX509Certificate&);
    void StartClientHandshake(TRequestStatus&);
    void StartServerHandshake(TRequestStatus&);

private:
    friend class CSocketTransfer;
    CHostTls(RSocket*, MGenericSecureSocket*);
    void ConstructL(const TDesC&);
    void Pump();
    void Transferred(TBool, TInt, const TDesC8&);
    void Fail(TInt);
    void Receive(TDes8&, TRequestStatus&, TSockXfrLength*, TBool);
    void SendData(const TDesC8&, TRequestStatus&, TSockXfrLength*);
    TInt Command(TUint, const TDesC8* = NULL, TDes8* = NULL);
    TInt SetHostname(const TDesC8&);
    static void Complete(TRequestStatus*&, TInt);
    static void Reject(TRequestStatus&, TInt);
    static TInt Error(TInt);
    RSocket* iSocket;
    MGenericSecureSocket* iGeneric;
    CSocketTransfer* iReader;
    CSocketTransfer* iWriter;
    TInt iHandle;
    TBuf8<253> iHostname;
    TBuf<32> iProtocol;
    TDialogMode iDialogMode;
    TRequestStatus* iHandshakeStatus;
    TRequestStatus* iReadStatus;
    TRequestStatus* iWriteStatus;
    TDes8* iReadBuffer;
    TSockXfrLength* iReadLength;
    HBufC8* iWriteBuffer;
    TSockXfrLength* iWriteLength;
    TInt iWriteOffset;
    CX509Certificate* iCertificate;
    TBool iReady;
    TBool iOneOrMore;
    TBool iEof;
};
#endif
