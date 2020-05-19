# Copyright (c) 2018 EKA2L1 Team.
# 
# This file is part of EKA2L1 project.
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
from enum import IntEnum
from symemu2.ipc.context import Context


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
def emulatorSystemCallInvoke(svcNum, time=0):
    def invokeDecorator(funcToInvoke):
        def funcWrapper():
            return funcToInvoke

        symemu.registerSvcInvokement(svcNum, time, funcToInvoke)

        return funcWrapper

    return invokeDecorator


def emulatorEpocFunctionInvoke(libname, ord, process_uid):
    def invokeDecorator(funcToInvoke):
        def funcWrapper():
            return funcToInvoke

        symemu.registerLibraryInvokement(libname, ord, process_uid, funcToInvoke)

        return funcWrapper

    return invokeDecorator


def emulatorBreakpointInvoke(imageName, addr, processUid):
    def invokeDecorator(funcToInvoke):
        def funcWrapper():
            return funcToInvoke

        symemu.registerBreakpointInvokement(imageName, addr, processUid, funcToInvoke)

        return funcWrapper

    return invokeDecorator


# The function registed with this decorator will be invoked when
# a reschedule happens
def emulatorRescheduleInvoke(func):
    def funcWrapper():
        return func

    symemu.registerRescheduleInvokement(func)

    return funcWrapper


class IpcInvokementType(IntEnum):
    SEND = 0
    COMPLETE = 2


# The function registered with this decorator will be invoked when a specific
# message (with requested opcode) is sent (a)sync/complete to specified server
def emulatorIpcInvoke(serverName, opcode, invokeType=IpcInvokementType.SEND):
    def invokeDecorator(funcToInvoke):
        def funcWrapper(context):
            return funcToInvoke

        def assembleSend(arg0, arg1, arg2, arg3, flags, process):
            funcToInvoke(Context(arg0, arg1, arg2, arg3, flags, process))

        def assembleComplete(msg):
            funcToInvoke(Context.makeFromMessage(msg))

        if invokeType == IpcInvokementType.SEND:
            wrapperToSend = assembleSend
        else:
            wrapperToSend = assembleComplete

        symemu.registerIpcInvokement(serverName, opcode, int(invokeType), wrapperToSend)

        return funcWrapper

    return invokeDecorator
