--- **Mem** module provides read/write operations on current memory address space.
---
--- All the functions in here can also be executed in another way, by getting the current process
--- object in `kernel` module and call read/write functions.
--- 
--- Invalid memory read will return 0, which is the limitation of this module. But write operations
--- can return a boolean indicates failure or success.
---
--- @module mem

mem = {}

local ffi = require('ffi')

pcall(ffi.load, 'native-lib', true)

ffi.cdef([[
    uint8_t eka2l1_mem_read_byte(const uint32_t addr);
    uint16_t eka2l1_mem_read_word(const uint32_t addr);
    uint32_t eka2l1_mem_read_dword(const uint32_t addr);
    uint64_t eka2l1_mem_read_qword(const uint32_t addr);

    int32_t eka2l1_mem_write_byte(const uint32_t addr, const uint8_t data);
    int32_t eka2l1_mem_write_word(const uint32_t addr, const uint16_t data);
    int32_t eka2l1_mem_write_dword(const uint32_t addr, const uint32_t data);
    int32_t eka2l1_mem_write_qword(const uint32_t addr, const uint64_t data);
]])

--- Read the byte value *(8-bit)* at the specified address.
--- @param addr The address to perform the read.
--- @return Byte value stored at the given address.
function mem.readByte(addr)
    return ffi.C.eka2l1_mem_read_byte(addr)
end

--- Read the word value *(16-bit)* at the specified address.
--- @param addr The address to perform the read.
--- @return Word value stored at the given address.
function mem.readWord(addr)
    return ffi.C.eka2l1_mem_read_word(addr)
end

--- Read the dword value *(32-bit)* at the specified address.
--- @param addr The address to perform the read.
--- @return Dword value stored at the given address.
function mem.readDword(addr)
    return ffi.C.eka2l1_mem_read_dword(addr)
end

--- Read the qword value *(64-bit)* at the specified address.
--- @param addr The address to perform the read.
--- @return Qword value stored at the given address.
function mem.readQword(addr)
    return ffi.C.eka2l1_mem_read_qword(addr)
end

--- Write a byte *(8-bit)* to the specified address.
--- @param addr The address to perform the read.
--- @param data The byte to write.
--- @return True if the write operation performed successfully.
function mem.writeByte(addr, data)
    return (ffi.C.eka2l1_mem_write_byte(addr, data) == 0) and false or true
end

--- Write a word *(16-bit)* to the specified address.
--- @param addr The address to perform the read.
--- @param data The byte to write.
--- @return True if the write operation performed successfully.
function mem.writeWord(addr, data)
    return (ffi.C.eka2l1_mem_write_word(addr, data) == 0) and false or true
end

--- Write a dword *(32-bit)* to the specified address.
--- @param addr The address to perform the read.
--- @param data The byte to write.
--- @return True if the write operation performed successfully.
function mem.writeDword(addr, data)
    return (ffi.C.eka2l1_mem_write_dword(addr, data) == 0) and false or true
end

--- Write a qword *(64-bit)* to the specified address.
--- @param addr The address to perform the read.
--- @param data The byte to write.
--- @return True if the write operation performed successfully.
function mem.writeQword(addr, data)
    return (ffi.C.eka2l1_mem_write_qword(addr, data) == 0) and false or true
end

return mem