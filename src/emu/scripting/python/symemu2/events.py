# Copyright (c) 2018 EKA2L1 Team.
# 
# This file is part of EKA2L1 project 
# (see bentokun.github.com/EKA2L1).
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

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
# a system call happens. A svc number is passed to function.
#
# Also, an optional time argument is available.
# - 0 means this hook will be invoked before executing this system call.
# - 1 means this hook will be invoked after executing this system call.
def emulatorSystemCallInvoke(svcNum, time = 0):
    def invokeDecorator(funcToInvoke):
        def funcWrapper():
            return funcToInvoke

        symemu.registerSvcInvokement(svcNum, time, funcToInvoke)

        return funcWrapper

    return invokeDecorator
    
def emulatorEpocFunctionInvoke(libname, ord):
    def invokeDecorator(funcToInvoke):
        def funcWrapper():
            return funcToInvoke

        symemu.registerLibraryInvokement(libname, ord, funcToInvoke)

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
