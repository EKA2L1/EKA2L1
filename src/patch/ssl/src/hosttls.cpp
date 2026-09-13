// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hosttls.h"
#include "dispatch.h"
#include <ssl.h>
#include <badesca.h>

const TUint KHostServerNameIndication = 0x40a;

class CSocketTransfer : public CActive {
public:
    CSocketTransfer(CHostTls& aOwner, TBool aSending)
        : CActive(EPriorityStandard), iOwner(aOwner), iSending(aSending) {
        CActiveScheduler::Add(this);
    }
    ~CSocketTransfer() { Cancel(); }
    void Start() {
        if (IsActive()) return;
        if (iSending) {
            if (iOwner.Command(ETlsDrain, NULL, &iBuffer) <= 0) return;
            if (iOwner.iSocket) iOwner.iSocket->Send(iBuffer, 0, iStatus);
            else iOwner.iGeneric->Send(iBuffer, 0, iStatus);
        } else {
            iBuffer.Zero();
            if (iOwner.iSocket) iOwner.iSocket->RecvOneOrMore(iBuffer, 0, iStatus, iLength);
            else iOwner.iGeneric->Read(iBuffer, iStatus);
        }
        SetActive();
    }
private:
    void RunL() { iOwner.Transferred(iSending, iStatus.Int(), iBuffer); }
    void DoCancel() {
        if (iSending) {
            if (iOwner.iSocket) iOwner.iSocket->CancelSend();
            else iOwner.iGeneric->CancelSend();
        } else {
            if (iOwner.iSocket) iOwner.iSocket->CancelRecv();
            else iOwner.iGeneric->CancelRead();
        }
    }
    CHostTls& iOwner;
    TBool iSending;
    TBuf8<16384> iBuffer;
    TSockXfrLength iLength;
};

CHostTls::CHostTls(RSocket* aSocket, MGenericSecureSocket* aGeneric)
    : iSocket(aSocket), iGeneric(aGeneric), iDialogMode(EDialogModeUnattended) {}

void CHostTls::ConstructL(const TDesC& aProtocol) {
    User::LeaveIfError(SetProtocol(aProtocol));
    iReader = new(ELeave) CSocketTransfer(*this, EFalse);
    iWriter = new(ELeave) CSocketTransfer(*this, ETrue);
}

EXPORT_C MSecureSocket* CHostTls::NewL(RSocket& aSocket, const TDesC& aProtocol) {
    CHostTls* self = new(ELeave) CHostTls(&aSocket, NULL);
    CleanupStack::PushL(self);
    self->ConstructL(aProtocol);
    CleanupStack::Pop(self);
    return self;
}

EXPORT_C MSecureSocket* CHostTls::NewL(MGenericSecureSocket& aSocket, const TDesC& aProtocol) {
    CHostTls* self = new(ELeave) CHostTls(NULL, &aSocket);
    CleanupStack::PushL(self);
    self->ConstructL(aProtocol);
    CleanupStack::Pop(self);
    return self;
}

EXPORT_C void CHostTls::UnloadDll(TAny*) {}

CHostTls::~CHostTls() {
    CancelAll();
    delete iReader;
    delete iWriter;
    delete iCertificate;
}

TInt CHostTls::Command(TUint aOperation, const TDesC8* aInput, TDes8* aOutput) {
    return ETlsCommand(0, iHandle, aOperation, aInput, aOutput);
}

void CHostTls::Complete(TRequestStatus*& aStatus, TInt aError) {
    if (aStatus) User::RequestComplete(aStatus, aError);
}

void CHostTls::Reject(TRequestStatus& aStatus, TInt aError) {
    aStatus = KRequestPending;
    TRequestStatus* status = &aStatus;
    Complete(status, aError);
}

TInt CHostTls::Error(TInt aError) {
    switch (aError) {
    case ETlsClosed: return KErrEof;
    case ETlsVerificationFailed: return KErrSSLInvalidCert;
    case ETlsInvalidState: return KErrNotReady;
    case ETlsFailed: return KErrSSLAlertHandshakeFailure;
    default: return aError;
    }
}

void CHostTls::Fail(TInt aError) {
    if (iReader) iReader->Cancel();
    if (iWriter) iWriter->Cancel();
    iReady = EFalse;
    if (iReadLength && iReadBuffer) (*iReadLength)() = iReadBuffer->Length();
    if (iWriteLength) (*iWriteLength)() = iWriteOffset;
    iReadBuffer = NULL;
    iReadLength = NULL;
    iWriteLength = NULL;
    delete iWriteBuffer;
    iWriteBuffer = NULL;
    Complete(iHandshakeStatus, aError);
    Complete(iReadStatus, aError);
    Complete(iWriteStatus, aError);
    if (iHandle > 0) ETlsDestroy(0, iHandle);
    iHandle = 0;
    delete iCertificate;
    iCertificate = NULL;
}

void CHostTls::StartClientHandshake(TRequestStatus& aStatus) {
    if (iHandle) { Reject(aStatus, KErrInUse); return; }
    if (!iHostname.Length()) { Reject(aStatus, KErrArgument); return; }
    TInt handle = ETlsCreate(0, &iHostname);
    if (handle < 0) { Reject(aStatus, handle); return; }
    iHandle = handle;
    iEof = EFalse;
    aStatus = KRequestPending;
    iHandshakeStatus = &aStatus;
    Pump();
}

void CHostTls::Transferred(TBool aSending, TInt aError, const TDesC8& aData) {
    if (!iHandle) return;
    if (!aSending) {
        if (aData.Length()) {
            TInt result = Command(ETlsFeed, &aData);
            if (result < 0) { Fail(Error(result)); return; }
        }
        if (aError == KErrEof || (aError == KErrNone && !aData.Length())) {
            iEof = ETrue;
            Command(ETlsEof);
        } else if (aError != KErrNone) { Fail(aError); return; }
    } else if (aError != KErrNone) { Fail(aError); return; }
    Pump();
}

void CHostTls::Pump() {
    if (!iHandle) return;
    TBool needRead = EFalse;
    if (iHandshakeStatus && !iReady) {
        TInt result = Command(ETlsHandshake);
        if (result == KErrNone) iReady = ETrue;
        else if (result == ETlsWantRead) needRead = ETrue;
        else if (result != ETlsWantWrite) { Fail(Error(result)); return; }
    }
    if (iReady && iWriteStatus) {
        // Keep each plaintext slice stable across a TLS WANT_WRITE retry.
        while (iWriteOffset < iWriteBuffer->Length()) {
            TPtrC8 part = iWriteBuffer->Mid(iWriteOffset, Min(16384, iWriteBuffer->Length() - iWriteOffset));
            TInt result = Command(ETlsWrite, &part);
            if (result > 0) iWriteOffset += result;
            else if (result == ETlsWantRead) { needRead = ETrue; break; }
            else if (result == ETlsWantWrite) break;
            else { Fail(result ? Error(result) : KErrDisconnected); return; }
            if (Command(ETlsDrain) >= 65536) break;
        }
    }
    if (iReady && iReadStatus) {
        while (iReadBuffer->Length() < iReadBuffer->MaxLength()) {
            TPtr8 part(const_cast<TUint8*>(iReadBuffer->Ptr()) + iReadBuffer->Length(),
                0, Min(16384, iReadBuffer->MaxLength() - iReadBuffer->Length()));
            TInt result = Command(ETlsRead, NULL, &part);
            if (result > 0) {
                iReadBuffer->SetLength(iReadBuffer->Length() + result);
                if (iOneOrMore) break;
            } else if (result == ETlsWantRead) { needRead = ETrue; break; }
            else if (result == ETlsWantWrite) break;
            else if (result == ETlsClosed) {
                if (iReadLength) (*iReadLength)() = iReadBuffer->Length();
                iReadBuffer = NULL;
                iReadLength = NULL;
                Complete(iReadStatus, KErrEof);
                break;
            } else { Fail(Error(result)); return; }
        }
        if (iReadStatus && ((iOneOrMore && iReadBuffer->Length()) ||
            iReadBuffer->Length() == iReadBuffer->MaxLength())) {
            if (iReadLength) (*iReadLength)() = iReadBuffer->Length();
            iReadBuffer = NULL;
            iReadLength = NULL;
            Complete(iReadStatus, KErrNone);
        }
    }
    iWriter->Start();
    if (!iWriter->IsActive() && Command(ETlsDrain) == 0) {
        if (iReady) Complete(iHandshakeStatus, KErrNone);
        if (iWriteStatus && iWriteOffset == iWriteBuffer->Length()) {
            if (iWriteLength) (*iWriteLength)() = iWriteOffset;
            iWriteLength = NULL;
            delete iWriteBuffer;
            iWriteBuffer = NULL;
            Complete(iWriteStatus, KErrNone);
        }
    }
    if (needRead && !iEof) iReader->Start();
    else if (needRead && iEof) Fail(KErrDisconnected);
}

void CHostTls::Receive(TDes8& aBuffer, TRequestStatus& aStatus, TSockXfrLength* aLength, TBool aOneOrMore) {
    if (iReadStatus) { Reject(aStatus, KErrInUse); return; }
    if (!iReady || iHandshakeStatus) { Reject(aStatus, KErrNotReady); return; }
    aBuffer.Zero();
    if (aLength) (*aLength)() = 0;
    aStatus = KRequestPending;
    iReadStatus = &aStatus;
    iReadBuffer = &aBuffer;
    iReadLength = aLength;
    iOneOrMore = aOneOrMore;
    Pump();
}

void CHostTls::Recv(TDes8& aBuffer, TRequestStatus& aStatus) {
    Receive(aBuffer, aStatus, NULL, EFalse);
}
void CHostTls::RecvOneOrMore(TDes8& aBuffer, TRequestStatus& aStatus, TSockXfrLength& aLength) {
    Receive(aBuffer, aStatus, &aLength, ETrue);
}

void CHostTls::SendData(const TDesC8& aData, TRequestStatus& aStatus, TSockXfrLength* aLength) {
    if (iWriteStatus) { Reject(aStatus, KErrInUse); return; }
    if (!iReady || iHandshakeStatus) { Reject(aStatus, KErrNotReady); return; }
    iWriteBuffer = aData.Alloc();
    if (!iWriteBuffer) { Reject(aStatus, KErrNoMemory); return; }
    if (aLength) (*aLength)() = 0;
    iWriteLength = aLength;
    iWriteOffset = 0;
    aStatus = KRequestPending;
    iWriteStatus = &aStatus;
    Pump();
}
void CHostTls::Send(const TDesC8& aData, TRequestStatus& aStatus) {
    SendData(aData, aStatus, NULL);
}
void CHostTls::Send(const TDesC8& aData, TRequestStatus& aStatus, TSockXfrLength& aLength) {
    SendData(aData, aStatus, &aLength);
}

void CHostTls::CancelAll() { Fail(KErrCancel); }
void CHostTls::CancelHandshake() { if (iHandshakeStatus) CancelAll(); }
void CHostTls::CancelSend() { if (iWriteStatus) CancelAll(); }
void CHostTls::CancelRecv() {
    if (iReadLength && iReadBuffer) (*iReadLength)() = iReadBuffer->Length();
    iReadBuffer = NULL;
    iReadLength = NULL;
    Complete(iReadStatus, KErrCancel);
    // A completed transport read may already contain a partial TLS record.
    // Let its AO feed those bytes even after the application cancels its read.
}
void CHostTls::Close() {
    CancelAll();
    if (iSocket) iSocket->Close();
    else iGeneric->Close();
}
void CHostTls::StartServerHandshake(TRequestStatus& aStatus) { Reject(aStatus, KErrNotSupported); }
void CHostTls::RenegotiateHandshake(TRequestStatus& aStatus) { Reject(aStatus, KErrNotSupported); }
void CHostTls::FlushSessionCache() {}
const CX509Certificate* CHostTls::ClientCert() { return NULL; }
TClientCertMode CHostTls::ClientCertMode() { return EClientCertModeIgnore; }
TInt CHostTls::SetClientCert(const CX509Certificate&) { return KErrNotSupported; }
TInt CHostTls::SetClientCertMode(TClientCertMode) { return KErrNotSupported; }
TInt CHostTls::SetServerCert(const CX509Certificate&) { return KErrNotSupported; }
TInt CHostTls::AvailableCipherSuites(TDes8&) { return KErrNotSupported; }
TInt CHostTls::SetAvailableCipherSuites(const TDesC8&) { return KErrNotSupported; }
TDialogMode CHostTls::DialogMode() { return iDialogMode; }
TInt CHostTls::SetDialogMode(TDialogMode aMode) {
    if (aMode != EDialogModeAttended && aMode != EDialogModeUnattended) return KErrNotSupported;
    iDialogMode = aMode;
    return KErrNone;
}
TInt CHostTls::CurrentCipherSuite(TDes8& aCipher) {
    if (!iReady) return KErrNotReady;
    TInt result = Command(ETlsCipher, NULL, &aCipher);
    return result < 0 ? Error(result) : KErrNone;
}
TInt CHostTls::Protocol(TDes& aProtocol) {
    if (iReady) {
        TBuf8<32> protocol;
        TInt result = Command(ETlsProtocol, NULL, &protocol);
        if (result < 0) return Error(result);
        if (protocol.Length() > aProtocol.MaxLength()) return KErrOverflow;
        aProtocol.Copy(protocol);
    } else {
        if (iProtocol.Length() > aProtocol.MaxLength()) return KErrOverflow;
        aProtocol.Copy(iProtocol);
    }
    return KErrNone;
}
TInt CHostTls::SetProtocol(const TDesC& aProtocol) {
    if (iHandle) return KErrInUse;
    if (aProtocol.CompareF(_L("TLS1.0")) && aProtocol.CompareF(_L("SSL3.0")) &&
        aProtocol.CompareF(_L("TLS1.2")) && aProtocol.CompareF(_L("TLS1.3"))) return KErrNotSupported;
    // The guest's legacy protocol selection requests the host's modern TLS policy.
    iProtocol.Copy(aProtocol);
    return KErrNone;
}
TInt CHostTls::SetHostname(const TDesC8& aName) {
    if (iHandle) return KErrInUse;
    if (!aName.Length() || aName.Length() > iHostname.MaxLength() || aName.Locate(0) >= 0) return KErrArgument;
    iHostname.Copy(aName);
    return KErrNone;
}
TInt CHostTls::SetOpt(TUint aName, TUint aLevel, const TDesC8& aValue) {
    if (aLevel != KSolInetSSL) return iSocket ? iSocket->SetOpt(aName, aLevel, aValue) : iGeneric->SetOpt(aName, aLevel, aValue);
    if (aName == KSoSSLDomainName) return SetHostname(aValue);
    if (aName == KSoUseSSLv2Handshake) return KErrNone;
    if (aName == KHostServerNameIndication) {
        if (aValue.Length() != sizeof(CDesC8Array*)) return KErrArgument;
        CDesC8Array* names = NULL;
        Mem::Copy(&names, aValue.Ptr(), sizeof(names));
        if (!names) return KErrArgument;
        TInt result = names->Count() == 1 ? SetHostname((*names)[0]) : KErrNotSupported;
        if (result == KErrNone) delete names;
        return result;
    }
    if (aValue.Length() != sizeof(TInt)) return KErrArgument;
    TInt value;
    Mem::Copy(&value, aValue.Ptr(), sizeof(value));
    if (aName == KSoDialogMode) return SetDialogMode(static_cast<TDialogMode>(value));
    if (aName == KSoEnableNullCiphers && !value) return KErrNone;
    return KErrNotSupported;
}
TInt CHostTls::SetOpt(TUint aName, TUint aLevel, TInt aValue) {
    TPckgBuf<TInt> value(aValue);
    return SetOpt(aName, aLevel, value);
}
TInt CHostTls::GetOpt(TUint aName, TUint aLevel, TDes8& aValue) {
    if (aLevel != KSolInetSSL) return iSocket ? iSocket->GetOpt(aName, aLevel, aValue) : iGeneric->GetOpt(aName, aLevel, aValue);
    if (aName == KSoSSLServerCert) {
        if (!iReady) return KErrNotReady;
        TPckgBuf<const CX509Certificate*> certificate(ServerCert());
        if (aValue.MaxLength() < certificate.Length()) return KErrOverflow;
        aValue.Copy(certificate);
        return KErrNone;
    }
    if (aName == KSoCurrentCipherSuite) return CurrentCipherSuite(aValue);
    if (aName == KSoAvailableCipherSuites) return AvailableCipherSuites(aValue);
    if (aName == KSoDialogMode) {
        if (aValue.MaxLength() < static_cast<TInt>(sizeof(TInt))) return KErrOverflow;
        TPckgBuf<TInt> mode(iDialogMode);
        aValue.Copy(mode);
        return KErrNone;
    }
    return KErrNotSupported;
}
TInt CHostTls::GetOpt(TUint aName, TUint aLevel, TInt& aValue) {
    TPckgBuf<TInt> value;
    TInt result = GetOpt(aName, aLevel, value);
    if (result == KErrNone && value.Length() == sizeof(TInt)) aValue = value();
    return result;
}
const CX509Certificate* CHostTls::ServerCert() {
    if (!iReady) return NULL;
    if (!iCertificate) {
        TInt size = Command(ETlsCertificate);
        if (size <= 0 || size > 65536) return NULL;
        HBufC8* data = HBufC8::New(size);
        if (!data) return NULL;
        TPtr8 bytes = data->Des();
        if (Command(ETlsCertificate, NULL, &bytes) > 0) {
            TRAPD(error, iCertificate = CX509Certificate::NewL(bytes));
            if (error != KErrNone) iCertificate = NULL;
        }
        delete data;
    }
    return iCertificate;
}
