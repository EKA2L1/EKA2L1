import ctypes

import symemu
import symemu2.events

## What is this leave invoke ?
##
## Leave is not supposed to panic, it's a way to tell parent function that
## there is an error occur with it, and all jobs and resources must immidiately 
## be freed. The leave will combine with the cleanup stack and trap handler,
## create a powerful exception handling with stack and heap object.
##
## Because of that, we don't know what the code of the leave resulted, only
## the parent function know it. This is a hook to get the leave code, for debugging
## and reversing an app/game or a mechanism.
##
## Note1: 824932975 is the SID for User::Leave in EPOC9, for example. Entirely, this will 
## get the actual address in EPOC9 and put a breakpoint there.
##
## Note2: Breakpoints scripting only support in Unicorn JIT.

@symemu2.events.emulatorEpocFunctionInvoke(824932975)
def leaveHook():
    # r0, when begging the function, contains the leave code. User is a static class
    # Since the code is uint32 from C, it must be converted to signed for the leave code
    # to be visible
    leaveCode = ctypes.c_long(symemu.Cpu.getReg(0)).value
    symemu.emulog('Function leaved with code: {}', leaveCode)