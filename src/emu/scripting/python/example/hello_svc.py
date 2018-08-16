import symemu
import symemu2.events

@symemu2.events.emulatorSystemCallInvoke(0x800000)
def waitForAnyRequestHook():
    symemu.emulog('Just wait for request...., what do you still want?')
    
def entryScript():
    symemu.emulog('The entry! This means that I have been imported and survived!')