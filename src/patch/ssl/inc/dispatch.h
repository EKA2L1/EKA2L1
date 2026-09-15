// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EKA2L1_TLS_DISPATCH_H
#define EKA2L1_TLS_DISPATCH_H
#include <e32base.h>

extern "C" TInt ETlsCreate(TUint, const TDesC8* aHostname, const TDesC8* aPeerAddress);
extern "C" TInt ETlsDestroy(TUint, TInt aHandle);
extern "C" TInt ETlsCommand(TUint, TInt aHandle, TUint aOperation,
    const TDesC8* aInput, TDes8* aOutput);

enum TTlsOperation {
    ETlsHandshake, ETlsFeed, ETlsDrain, ETlsRead, ETlsWrite,
    ETlsProtocol, ETlsCipher, ETlsCertificate, ETlsClose, ETlsEof
};
enum TTlsResult {
    ETlsWantRead = -30000, ETlsWantWrite = -30001, ETlsClosed = -30002,
    ETlsVerificationFailed = -30003, ETlsInvalidState = -30004, ETlsFailed = -30005
};
#endif
