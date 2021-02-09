std = {}

local kernel = require('symemu.kernel')
local bitops = require('bit')

DESCRIPTOR_TYPE_BUF_CONST = 0
DESCRIPTOR_TYPE_PTR_CONST = 1
DESCRIPTOR_TYPE_PTR = 2
DESCRIPTOR_TYPE_BUF = 3
DESCRIPTOR_TYPE_BUF_CONST_PTR = 4
DESCRIPTOR_TYPE_MAX = 5

local descriptor = {}

function getPtrBufConstAddress(pr, address)
    return address + 4
end

function getPtrPtrConstAddress(pr, address)
    return pr:readDword(address + 4)
end

function getPtrBufAddress(pr, address)
    return address + 8
end

function getPtrPtrAddress(pr, address)
    return pr:readDword(address + 8)
end

function getPtrBufConstPtrAddress(pr, address)
    local realBufAddr = pr:readDword(address + 8)
    return realBufAddr + 4
end

function descriptor:new(pr, addr)
    local o = {}

    local addressLookupTable = {
        [DESCRIPTOR_TYPE_BUF_CONST] = getPtrBufConstAddress,
        [DESCRIPTOR_TYPE_BUF_CONST_PTR] = getPtrBufConstPtrAddress,
        [DESCRIPTOR_TYPE_BUF] = getPtrBufAddress,
        [DESCRIPTOR_TYPE_PTR] = getPtrPtrAddress,
        [DESCRIPTOR_TYPE_PTR_CONST] = getPtrPtrConstAddress
    }

    local desword = process:readDword(addr)

    self.length = bitops.band(desword, 0xFFFFFFF);
    self.type = bitops.band(bitops.rhsift(desword, 28), 0xF)

    if self.type >= DESCRIPTOR_TYPE_MAX then
        error('Descriptor type is invalid!')
    else
        self.dataAddr = addressLookupTable[self.type](pr, addr)
        self.pr = pr
    end
    
    setmetatable(o, self)
    self.__index = self

    return o
end

local descriptor8 = {}

function descriptor8:new(pr, addr)
    local o = descriptor:new(pr, addr)

    setmetatable(o, self)
    self.__index = self

    return o
end

setmetatable(descriptor8, { __index = descriptor })

function descriptor8:rawData()
    return self.pr:readMemory(self.dataAddr, self.length)
end

function std.makeDescriptor8(pr, addr)
    return descriptor8:new(pr, addr)
end

local descriptor16 = {}

function descriptor16:new(pr, addr)
    local o = descriptor:new(pr, addr)

    setmetatable(o, self)
    self.__index = self

    return o
end

setmetatable(descriptor16, { __index = descriptor })

function descriptor16:rawData()
    return self.pr:readMemory(self.dataAddr, self.length * 2)
end

function std.makeDescriptor16(pr, addr)
    return descriptor16:new(pr, addr)
end

-- Request status implementation
local requestStatus = {}

function requestStatus:new(pr, addr)
    o = {}

    setmetatable(o, self)
    self.__index = self
    self.ownProcess = pr
    self.dataAddr = addr

    return o
end

function requestStatus:value()
    return self.pr:readDword(addr)
end

function requestStatus:flags()
    return self.pr:readDword(addr + 4)
end

function std.makeRequestStatus(pr, addr)
    return requestStatus:new(pr, addr)
end
-- End request status implementation

return std