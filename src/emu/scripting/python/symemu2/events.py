# Decorator for event register

import symemu

def emulatorPanicInvoke(panicName):
    def invokeDecorator(funcToInvoke):
        def funcWrapper(errorCode):
            pass

        # Register invoke here and return a empty FuncWrapper function
        symemu.registerPanicInvokement(panicName, funcToInvoke)

        return funcWrapper

    return invokeDecorator

def emulatorSystemCallInvoke(svcNum):
    def invokeDecorator(funcToInvoke):
        def funcWrapper():
            pass

        symemu.registerSvcInvokement(svcNum, funcToInvoke)

        return funcWrapper

    return invokeDecorator