// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EKA2L1_GENERIC_SOCKET_H
#define EKA2L1_GENERIC_SOCKET_H
#include <es_sock.h>

// Symbian 9.2+ GenericSecureSocket.h ABI, absent from the public S60 SDK.
class MGenericSecureSocket {
public:
    virtual void Send(const TDesC8&, TUint, TRequestStatus&) = 0;
    virtual void CancelSend() = 0;
    virtual void Recv(TDes8&, TUint, TRequestStatus&) = 0;
    virtual void CancelRecv() = 0;
    virtual void Read(TDes8&, TRequestStatus&) = 0;
    virtual void CancelRead() = 0;
    virtual TInt SetOpt(TUint, TUint, const TDesC8&) = 0;
    virtual TInt GetOpt(TUint, TUint, TDes8&) = 0;
    virtual void LocalName(TSockAddr&) = 0;
    virtual void RemoteName(TSockAddr&) = 0;
    virtual void Close() = 0;
};
#endif
