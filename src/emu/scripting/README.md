- EKA2L1 can be scipted in Python. There are two things you need to know
    - symemu: is a embed module that exists within each EKA2L1 instance. It lets you access to core feature
    - symemu2: is a set of folder contains utility that provides from EKA2L1's common and other libraries, intended for miscs use

## Symemu
### Fundamental
   - When a script is imported, scriptEntry function are immidiately invoked (if it's available), to setup the script.
   - You can script a function to be called at specific emulation event. The event may be panic, or an interrupt. Either way, there
is different way to do it.
   - The most easy way is to assign each function with a @EmulationInvokeEvent decorator. There are three types of invoke hook:
       - **emulatorSystemCallInvoke**
       - **emulatorPanicInvoke**
       - **emulatorRescheduleInvoke**
   - See examples (*hello* files), for more information.
   - Scripts should be in the scripts folder. The *scripts* folder must be in the same directory as executable.

### Logging
   - Logging has the similar syntax like in Python. You can preformat it first or let EKA2L1 do it for you
   - Use logging through function **emulog**. For e.g . **emulog('Hi, Hello EkA2L1, i just pee xd')**

### Process
   - Access to process is freely. You can read or write to an address space, if you can confirm it's committed.
   - You can get a list of process running from symemu.getProcessesList. Iterate through each process, you can get its name, its path, etc

### Limitation
   - A system call hook may only be catched with only one OS version or all of them, because system call number are different in various
implementation of Symbian. You should make multiple hook for each OS version or not if you can confirm the number is not going to be changed.

## Symemu2
   - Unlike Symemu, Symemu2 is compiled to a pyd and not related at all to Symemu. It's a standalone, and provides useful utilities
to deal with Symbian stuff. For example, you can use to extract a Symbian game information
   - Some modules are written by hand that will help with scripting
   
## FAQ
- Q: When does a script getting initialized?
- A: After the emulator done intialization, meaning at the start of EKA2L1, *scriptEntry* will be called.

- Q: Will hook script run asynchronously or synchronously?
- A: Synchronously. The slowdown is possible if there are many scripts hooked to the same thing or if script is doing something big.

- Q: Can i make script function running at other time on emulator rather than reschedule, svc, etc ?
- A: More hook will be added in the future. Other than, no.
