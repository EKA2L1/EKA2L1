/*
 * main.cpp
 *
 *  Created on: Feb 21, 2019
 *      Author: fewds
 */

#include <e32base.h>
#include <e32std.h>
#include <e32cmn.h>
#include <w32std.h>
#include <e32debug.h>
#include <eikon.hrh>

class CActiveInputHandler: public CActive
{
  RWsSession *session;
  RWindowGroup *group;
  
  TWsEvent event;
  
public:
  explicit CActiveInputHandler(RWsSession *session, RWindowGroup *group)
    : CActive(EPriorityStandard), session(session), group(group) 
  {
    CActiveScheduler::Add(this);
  }
  
  void StartReceiveInput()
  {
    Cancel();
    session->EventReady(&iStatus);
    SetActive();
  }
  
  void RunL() 
  {
    session->GetEvent(event);
    
    switch (event.Type())
    {
    case EEventKeyDown: case EEventKeyUp:
    {
      TKeyEvent *keyEvt = event.Key();
      RDebug::Printf("Key down/up code %d scancode %d", keyEvt->iCode, keyEvt->iScanCode);

      break;
    }
    
    case EEventKey:
    {
      TKeyEvent *keyEvt = event.Key();
      RDebug::Printf("Key code %d scancode %d", keyEvt->iCode, keyEvt->iScanCode);

      break;
    }
    
    case EEventFocusGained:
    {
      RDebug::Printf("Focus gained!");
      break;
    }
    
    case EEventFocusLost:
    {
      RDebug::Printf("Focus lost!");
      break;
    }
    
    case EEventPointerEnter:
	{
      RDebug::Printf("Pointer enters the chat");
      break;
	}

    case EEventPointerExit:
	{
      RDebug::Printf("Pointer exits the chat");
      break;
	}
	
    case EEventPointer: {
      TPointerEvent *pointerEvt = event.Pointer();
      RDebug::Printf("Pointer event type: %d, position (%d, %d)", pointerEvt->iType, pointerEvt->iPosition.iX,
        pointerEvt->iPosition.iY);
      
	  break;
    }
	
    default:
    {
      break;
    }
    }
    
    session->EventReady(&iStatus);
    SetActive();
  }
  
  void DoCancel()
  {
    session->EventReadyCancel();
  }
};

void MainL()
{
  // Setup the active scheduler, push it up
  CActiveScheduler *sched = new (ELeave) CActiveScheduler();
  CleanupStack::PushL(sched);
  
  // Install the scheduler
  CActiveScheduler::Install(sched);
  
  RFs fs;
  User::LeaveIfError(fs.Connect(-1));
  
  fs.SetSessionToPrivate(EDriveC);
  
  RWsSession winSession;
  User::LeaveIfError(winSession.Connect(fs));
  
  CWsScreenDevice *device = new (ELeave) CWsScreenDevice(winSession);
  CleanupStack::PushL(device);
  device->Construct();
  
  RWindowGroup *group = new (ELeave) RWindowGroup(winSession);
  CleanupStack::PushL(group);
  
  // Capture key handle. Don't need that now
  group->Construct(0, true);
  RDebug::Printf("Key capture OK handle: %d", group->CaptureKey(EKeyOK, 0, 0));
  
  RBlankWindow *window = new (ELeave) RBlankWindow(winSession);
  window->Construct(*group, 123);
  TInt err = window->SetSizeErr(device->SizeInPixels());
  
  window->PointerFilter(EPointerFilterDrag, 0);
  window->SetPointerCapture(RWindowBase::TCaptureDragDrop);
  window->Activate();
  
  CActiveInputHandler *handler = new (ELeave) CActiveInputHandler(&winSession, group);
  CleanupStack::PushL(handler);
  
  handler->StartReceiveInput();
  CActiveScheduler::Start();
  
  CleanupStack::PopAndDestroy(5);
}

TInt E32Main()
{ 
  CTrapCleanup::New();
  TRAPD(err, MainL());
  
  return err;
}
