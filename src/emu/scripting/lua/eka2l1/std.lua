std = {}

local kernel = require('eka2l1.kernel')
local helper = require('eka2l1.helper')
local bitops = require('bit')
local ffi = require('ffi')

pcall(ffi.load, 'native-lib', true)

ffi.cdef([[
    const char *eka2l1_std_utf16_to_utf8(const char *wstring, const int length);
    void eka2l1_free_string(const char* str);
]])

std.DESCRIPTOR_TYPE_BUF_CONST = 0
std.DESCRIPTOR_TYPE_PTR_CONST = 1
std.DESCRIPTOR_TYPE_PTR = 2
std.DESCRIPTOR_TYPE_BUF = 3
std.DESCRIPTOR_TYPE_BUF_CONST_PTR = 4
std.DESCRIPTOR_TYPE_MAX = 5

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

std.descriptor = helper.class(function(self, pr, addr)
    local addressLookupTable = {
        [DESCRIPTOR_TYPE_BUF_CONST] = getPtrBufConstAddress,
        [DESCRIPTOR_TYPE_BUF_CONST_PTR] = getPtrBufConstPtrAddress,
        [DESCRIPTOR_TYPE_BUF] = getPtrBufAddress,
        [DESCRIPTOR_TYPE_PTR] = getPtrPtrAddress,
        [DESCRIPTOR_TYPE_PTR_CONST] = getPtrPtrConstAddress
    }

    local desword = pr:readDword(addr)

    self.length = bitops.band(desword, 0xFFFFFFF);
    self.type = bitops.band(bitops.rshift(desword, 28), 0xF)

    if self.type >= DESCRIPTOR_TYPE_MAX then
        error('Descriptor type is invalid!')
    else
        self.dataAddr = addressLookupTable[self.type](pr, addr)
        self.pr = pr
    end
end)

std.descriptor8 = helper.class(descriptor, function(self, pr, addr)
    descriptor.init(self, pr, addr)
end)

function std.descriptor8:rawData()
    return self.pr:readMemory(self.dataAddr, self.length)
end

function std.descriptor8:__tostring()
    return ffi.string(self:rawData())
end

std.descriptor16 = helper.class(descriptor, function(self, pr, addr)
    descriptor.init(self, pr, addr)
end)

function std.rawUtf16ToString(raw, length)
    local result = ffi.C.eka2l1_std_utf16_to_utf8(raw, length)
    ffi.gc(result, ffi.C.eka2l1_free_string)

    return ffi.string(result)
end

function std.descriptor16:rawData()
    return self.pr:readMemory(self.dataAddr, self.length * 2)
end

function std.descriptor16:__tostring()
	return std.rawUtf16ToString(self:rawData(), self.length)
end

-- Request status implementation
std.requestStatus = helper.class(function(pr, addr)
    self.ownProcess = pr
    self.dataAddr = addr
end)

function std.requestStatus:value()
    return self.pr:readDword(addr)
end

function std.requestStatus:flags()
    return self.pr:readDword(addr + 4)
end
-- End request status implementation

return std