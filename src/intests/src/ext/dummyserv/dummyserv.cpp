#include <intests/ext/dummyserv/dummyserv.h>
#include <intests/ext/dummyserv/dummyservession.h>

#include <e32base.h>
#include <e32std.h>

CDummyServer *CDummyServer::NewLC()
    {
        CDummyServer *self = new (ELeave) CDummyServer(EPriorityNormal);
        CleanupStack::PushL(self);
        self->ConstructL();
        
        return self;
    }

CDummyServer *CDummyServer::NewL()
    {
        CDummyServer *self = NewLC();
        CleanupStack::Pop(self);
        
        return self;
    }

CDummyServer::CDummyServer(TInt aPriority)
    :CServer2(aPriority)
    {
    
    }

void CDummyServer::ConstructL()
    {
        StartL(KDummyServName);
    }


CSession2 *CDummyServer::NewSessionL(const TVersion &aVersion, const RMessage2 &aMsg) const
    {
        return CDummyServerSession::NewL(*const_cast<CDummyServer*>(this));
    }
