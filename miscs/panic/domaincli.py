import symemu

def panicHook(panicCode):
    errcode = panicCode & $FFFF
    line = (panicCode >> 16) & $FFFF
	
    emulog('DomainClient exited with exit code: 0x{:x} at line {}', errcode, line)
	