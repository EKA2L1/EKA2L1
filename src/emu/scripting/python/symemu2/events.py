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

class IpcInvokementType(IntEnum):
    SEND = 0
    COMPLETE = 2


# The function registered with this decorator will be invoked when a specific
# message (with requested opcode) is sent (a)sync/complete to specified server
def emulatorIpcInvoke(serverName, opcode, invokeType=IpcInvokementType.SEND):
    def invokeDecorator(funcToInvoke):
        def funcWrapper(context):
            return funcToInvoke

        def assembleSend(arg0, arg1, arg2, arg3, flags, reqstsaddr, process):
            funcToInvoke(Context(opcode, arg0, arg1, arg2, arg3, flags, reqstsaddr, process))

        def assembleComplete(msg):
            funcToInvoke(Context.makeFromMessage(msg))

        if invokeType == IpcInvokementType.SEND:
            wrapperToSend = assembleSend
        else:
            wrapperToSend = assembleComplete

        symemu.registerIpcInvokement(serverName, opcode, int(invokeType), wrapperToSend)

        return funcWrapper

    return invokeDecorator
