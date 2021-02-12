IpcContext = {}

local kernel = require('symemu.kernel')
local std = require('symemu.std')
local common = require('symemu.common')
local bitops = require('bit')

IPC_ARGUMENT_TYPE_UNSPECIFIED = 0
IPC_ARGUMENT_TYPE_HANDLE = 1
IPC_ARGUMENT_TYPE_DES8 = 4
IPC_ARGUMENT_TYPE_DES16 = 5
IPC_ARGUMENT_TYPE_DESC8 = 6
IPC_ARGUMENT_TYPE_DESC16 = 7

local arguments = {}

function arguments:new(arg1, arg2, arg3, arg4, flags)
    local o = {}

    self.args = {}
    self.args[1] = arg1
    self.args[2] = arg2
    self.args[3] = arg3
    self.args[4] = arg4
    self.flags = flags

    setmetatable(o, self)
    self.__index = self

    return o
end

function arguments:valueType(idx)
    if (idx < 0) or (idx > 3) then
        error('Argument index out of range!')
    end

    return bitops.band(bitops.rshift(self.flags, idx * 3), 7)
end

function arguments:value(idx)
    if (idx < 0) or (idx > 3) then
        error('Argument index out of range!')
    end

    return self.args[idx]
end

local context = {}

function context:new(func, arg0, arg1, arg2, arg3, flags, reqsts, sender)
    local o = {}

    self.args = arguments:new(arg0, arg1, arg2, arg3, flags)
    self.func = func
    self.sender = sender
    self.requestStatus = reqsts

    setmetatable(o, self)
    self.__index = self

    return o
end

function context:rawArgument(idx)
    return self.args.value(idx)
end

function context:ownThread()
    return self.sender
end

function context:argument(self, idx)
    local argtype = self.args.valueType(idx)
    local value = self.args.value(idx)

    if (argtype == IPC_ARGUMENT_TYPE_UNSPECIFIED) or (IPC_ARGUMENT_TYPE_HANDLE) then
        return value
    end

    if (argtype == IPC_ARGUMENT_TYPE_DES8) or (argtype == IPC_ARGUMENT_TYPE_DESC8) then
        return std.makeDescriptor8(self.sender:ownProcess(), value)
    end

    return std.makeDescriptor16(self.sender:ownProcess(), value)
end

function IpcContext.makeFromMessage(msg)
    return context:new(msg:func(), msg:arg(0), msg:arg(1), msg:arg(2), msg:arg(3), msg:flags(),
        std.makeRequestStatus(msg:sender():ownProcess(), msg:requestStatusAddress()), msg:sender())
end

function IpcContext.makeFromValues(func, arg0, arg1, arg2, arg3, flags, reqsts, sender)
    return context:new(func, arg0, arg1, arg2, arg3, flags, std.makeRequestStatus(sender:ownProcess(), reqsts), sender)
end

return IpcContext