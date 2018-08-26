# Decorator for event register

import symemu

# The function registed with this decorator will be invoked when
# a panic happens to a thread. A panic code going to be passed
# This still stands for N-Gage 2.0 apps, where panic code are sandbox-catched
def emulatorPanicInvoke(panicName):
    def invokeDecorator(funcToInvoke):
        def funcWrapper(errorCode):
            return funcToInvoke

        # Register invoke here and return a empty FuncWrapper function
        symemu.registerPanicInvokement(panicName, funcToInvoke)

        return funcWrapper

    return invokeDecorator
    
# The function registed with this decorator will be invoked when
# a system call happens. A svc number is passed to function
def emulatorSystemCallInvoke(svcNum):
    def invokeDecorator(funcToInvoke):
        def funcWrapper():
            return funcToInvoke

        symemu.registerSvcInvokement(svcNum, funcToInvoke)

        return funcWrapper

    return invokeDecorator
    
def emulatorEpocFunctionInvoke(sid):
    def invokeDecorator(funcToInvoke):
        def funcWrapper():
            return funcToInvoke

        symemu.registerSidInvokement(sid, funcToInvoke)

        return funcWrapper

    return invokeDecorator
    
def emulatorBreakpointInvoke(addr):
    def invokeDecorator(funcToInvoke):
        def funcWrapper():
            return funcToInvoke

        symemu.registerBreakpointInvokement(addr, funcToInvoke)

        return funcWrapper

    return invokeDecorator

# The function registed with this decorator will be invoked when
# a reschedule happens
def emulatorRescheduleInvoke(func):
    def funcWrapper():
        return func

    symemu.registerRescheduleInvokement(func)

    return funcWrapper