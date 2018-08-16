import symemu
import symemu2.events

@symemu2.events.emulatorPanicInvoke('domainCli.cpp')
def domainClientPanic(panicCode):
    errcode = panicCode & 0xFFFF
    line = (panicCode >> 16) & 0xFFFF
	
    symemu.emulog('DomainClient exited with exit code: {} at line {}', errcode, line)