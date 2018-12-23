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
    : iServer(aServer)
    {
    }

void CDummyServerSession::ConstructL()
    {
        
    }

CDummyServerSession *CDummyServerSession::NewL(CDummyServer &aServer)
    {
        CDummyServerSession *self = NewLC(aServer);
        CleanupStack::Pop(self);
        
        return self;
    }

CDummyServerSession *CDummyServerSession::NewLC(CDummyServer &aServer)
    {
        CDummyServerSession *self = new (ELeave) CDummyServerSession(aServer);
        CleanupStack::PushL(self);
        self->ConstructL();
        
        return self;
    }

// Psttt
struct SSecertStruct 
    {
        TInt  iSecertNum;
        TInt  iSecertNum2;
    };

void CDummyServerSession::ServiceL(const RMessage2 &aMessage)
    {
        switch (aMessage.Function())
            {
            case EDummyOpHashFromNumber:
                {
                    // Get the number from package
                    TInt deNum;
                
                    TPckg<TInt> num(deNum);
                    aMessage.Read(0, num);
                    
                    deNum = (deNum << 16) + 0x5972;
                    
                    aMessage.Write(0, num);
                    aMessage.Complete(KErrNone);
                    
                    break;
                }
            
            case EDummyOpGetSecertString:
                {
                    _LIT(KSecertMessageFromMe, "Congratulations!");
                    aMessage.Write(0, KSecertMessageFromMe);
    
                    aMessage.Complete(KErrNone);
                    
                    break;
                }
            
            // Try write with offset
            case EDummyOpGetSecertPackage: 
                {
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
            
            default: 
                {
                    User::Panic(_L("DummyServForDummy"), KErrNotSupported);
                    break;
                }
            }
    }
