mem = {}

local ffi = require('ffi')

pcall(ffi.load, 'native-lib', true)

ffi.cdef([[
    uint8_t symemu_mem_read_byte(const uint32_t addr);
    uint16_t symemu_mem_read_word(const uint32_t addr);
    uint32_t symemu_mem_read_dword(const uint32_t addr);
    uint64_t symemu_mem_read_qword(const uint32_t addr);

    int32_t symemu_mem_write_byte(const uint32_t addr, const uint8_t data);
    int32_t symemu_mem_write_word(const uint32_t addr, const uint16_t data);
    int32_t symemu_mem_write_dword(const uint32_t addr, const uint32_t data);
    int32_t symemu_mem_write_qword(const uint32_t addr, const uint64_t data);
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

function mem.writeByte(addr, data)
    return (ffi.C.symemu_mem_write_byte(addr, data) == 0) and false or true
end

function mem.readWord(addr, data)
    return (ffi.C.symemu_mem_write_word(addr, data) == 0) and false or true
end

function mem.readDword(addr, data)
    return (ffi.C.symemu_mem_write_dword(addr, data) == 0) and false or true
end

function mem.readQword(addr, data)
    return (ffi.C.symemu_mem_write_qword(addr, data) == 0) and false or true
end

return mem