/*
 * dummyservsession.cpp
 *
 *  Created on: Dec 23, 2018
 *      Author: fewds
 */

#include <intests/ext/dummyserv/dummyservession.h>

#include <e32base.h>
#include <e32std.h>

CDummyServerSession::CDummyServerSession(CDummyServer &aServer)
    : iServer(aServer) {
}

void CDummyServerSession::ConstructL() {
}

CDummyServerSession *CDummyServerSession::NewL(CDummyServer &aServer) {
    CDummyServerSession *self = NewLC(aServer);
    CleanupStack::Pop(self);

    return self;
}

CDummyServerSession *CDummyServerSession::NewLC(CDummyServer &aServer) {
    CDummyServerSession *self = new (ELeave) CDummyServerSession(aServer);
    CleanupStack::PushL(self);
    self->ConstructL();

    return self;
}

// Psttt
struct SSecertStruct {
    TInt iSecertNum;
    TInt iSecertNum2;
};

void CDummyServerSession::ServiceL(const RMessage2 &aMessage) {
    switch (aMessage.Function()) {
    case EDummyOpHashFromNumber: {
        // Get the number from package
        TInt deNum;

        TPckg<TInt> num(deNum);
        aMessage.Read(0, num);

        deNum = (deNum << 16) + 0x5972;

        aMessage.Write(0, num);
        aMessage.Complete(KErrNone);

        break;
    }

    case EDummyOpGetSecertString: {
        _LIT(KSecertMessageFromMe, "Congratulations!");
        aMessage.Write(0, KSecertMessageFromMe);

        aMessage.Complete(KErrNone);

        break;
    }

    // Try write with offset
    case EDummyOpGetSecertPackage: {
        SSecertStruct thatssecert;
        thatssecert.iSecertNum = 12;
        thatssecert.iSecertNum2 = 70;

        TPckg<SSecertStruct> sosecert(thatssecert);

        TUint16 thatsmagic = 2019;
        TPckg<TUint16> somagic(thatsmagic);

        aMessage.Write(0, somagic, 0);
        aMessage.Write(0, sosecert, 4);

        aMessage.Complete(KErrNone);
        break;
    }

    // Take slot 0, divide it to two string: one with 2 characters,
    // one with length - 2, than put it in slot 1 and 2
    // KErrNoMemory if string length <= 2
    case EDummyOpDivideString: {
        TInt len = aMessage.GetDesLength(0);

        if (len <= 2) {
            aMessage.Complete(KErrNoMemory);
            break;
        }

        TBuf<2> string1;

        HBufC *string2Buf = HBufC::NewL(len - 2);
        TPtr string2 = string2Buf->Des();

        string1.SetLength(2);
        string2.SetLength(len - 2);

        aMessage.Read(0, string1, 0);
        aMessage.Read(0, string2, 2);

        TInt err = KErrNone;

        err = aMessage.Write(1, string1);
        err = aMessage.Write(2, string2);

        aMessage.Complete(err);
        break;
    }
    default: {
        User::Panic(_L("DummyServForDummy"), KErrNotSupported);
        break;
    }
    }
}
