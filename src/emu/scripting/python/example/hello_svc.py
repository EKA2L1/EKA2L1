import symemu
import symemu2.events
import symemu2.svc

@symemu2.events.emulatorSystemCallInvoke(symemu2.svc.Epoc9Svc.WaitForAnyRequest)
def waitForAnyRequestHook():
    symemu.emulog('Just wait for request...., what do you still want?')
    
def entryScript():
    symemu.emulog('The entry! This means that I have been imported and survived!')