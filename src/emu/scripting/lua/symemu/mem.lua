mem = {}

local ffi = require('ffi')

pcall(ffi.load, 'native-lib', true)

ffi.cdef([[
    uint8_t symemu_mem_read_byte(const uint32_t addr);
    uint16_t symemu_mem_read_word(const uint32_t addr);
    uint32_t symemu_mem_read_dword(const uint32_t addr);
    uint64_t symemu_mem_read_qword(const uint32_t addr);
]])

function mem.readByte(addr)
    return ffi.C.symemu_mem_read_byte(addr)
end

function mem.readWord(addr)
    return ffi.C.symemu_mem_read_word(addr)
end

function mem.readDword(addr)
    return ffi.C.symemu_mem_read_dword(addr)
end

function mem.readQword(addr)
    return ffi.C.symemu_mem_read_qword(addr)
end