# Copyright (c) 2020 EKA2L1 Team.
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

from enum import Enum

import ctypes
import symemu

from symemu2 import descriptor

class ArgumentType(Enum):
    UNSPECIFIED = 0
    HANDLE = 1
    DES8 = 4
    DES16 = 5
    DESC8 = 6
    DESC16 = 7

def getArgumentTypeFromFlags(flags, idx):
    if (idx > 3) or (idx < 0):
        raise IndexError('Index {} is invalid! The index must be between 0 and 3', idx)

    return ArgumentType(((flags >> (idx * 3)) & 7))

class Arguments(object):
    def __init__(self, arg0, arg1, arg2, arg3, flags):
        self.args = [ arg0, arg1, arg2, arg3 ]
        self.flags = flags
    
    def getArgumentType(self, idx):
        return getArgumentTypeFromFlags(self.flags, idx)

    def getArgument(self, idx):
        return self.args[idx]

class Context(object):
    def __init__(self, msg):
        self.args = Arguments(msg.arg(0), msg.arg(1), msg.arg(2), msg.arg(3), msg.flags())
        self.sender = msg.sender()
    
    def __init__(self, arg0, arg1, arg2, arg3, flags, sender):
        self.args = Arguments(arg0, arg1, arg2, arg3, flags)
        self.sender = sender

    def getArguments(self):
        return self.args

    # Get integer value in specified argument slot.
    #
    # Parameter
    # ----------------------
    # idx:      The argument slot index we want to retrieve the value from.
    #
    # Returns
    # ----------------------
    # int
    #   Integer value in the slot.
    def getArgumentRaw(self, idx):
        return self.args.getArgument(idx)

    # Get argument data from slot.
    #
    # If the argument in the given slot is a handle or unspecified, integer is returned.
    # Else, descriptor object will be returned.
    #
    # To get the real value of the argument slot, use get_argument_raw
    #
    # Parameter
    # ----------------------
    # idx:      The argument slot index we want to retrieve argument data from.
    def getArgument(self, idx):
        argtype = self.args.getArgumentType(idx)
        value = self.args.getArgument(idx)

        if (argtype == ArgumentType.UNSPECIFIED) or (argtype == ArgumentType.HANDLE):
            return value

        if (argtype == ArgumentType.DES8) or (argtype == ArgumentType.DESC8):
            return descriptor.Descriptor8(value, self.sender.getOwningProcess())

        return descriptor.Descriptor16(value, self.sender.getOwningProcess())