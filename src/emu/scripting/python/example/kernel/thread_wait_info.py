import symemu
import symemu2.events
import symemu2.svc

@symemu2.events.emulatorSystemCallInvoke(symemu2.svc.Epoc9Svc.WaitForAnyRequest)
def waitForRequestWhoHook():
    # Get current thread
    crrThread = symemu.getCurrentThread()
    symemu.emulog('Thread {} will wait for any request!'.format(crrThread.getName()))
