import symemu
import symemu2.events

# Demonstration of the API. This shouldn't be called every reschedule :P

@symemu2.events.emulatorRescheduleInvoke
def getProcess():
    processList = symemu.getProcessesList()
    
    for process in processList:
        symemu.emulog('Name: {}, Path: {}', process.getName(), process.getExecutablePath())