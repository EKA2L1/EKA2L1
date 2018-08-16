- EKA2L1 can be scipted in Python. There are two things you need to know
    - symemu: is a embed module that exists within each EKA2L1 instance. It lets you access to core feature
    - symemu2: is a set of folder contains utility that provides from EKA2L1's common and other libraries, intended for miscs use

## Symemu
### Fundamental
    - When a script is imported, scriptEntry function are immidiately invoked (if it's available), to setup the script.
    - You can script a function to be called at specific emulation event. The event may be panic, or an interrupt. Either way, there
is different way to do it.
    - The most easy way is to assign each function with a @EmulationInvokeEvent decorator. There are two types of invoke hook:
        - **emulatorSystemCallInvoke**
        - **emulatorPanicInvoke**
    - See examples (*hello* files), for more information.

### Logging
    - Logging has the similar syntax like in Python. You can preformat it first or let EKA2L1 do it for you
    - Use logging through function **emulog**. For e.g . **emulog('Hi, Hello EkA2L1, i just pee xd')**

## Symemu2
    - Unlike Symemu, Symemu2 is compiled to a pyd and not related at all to Symemu. It's a standalone, and provides useful utilities
to deal with Symbian stuff. For example, you can use to extract a Symbian game information
    - Some modules are written by hand an will help with scripting