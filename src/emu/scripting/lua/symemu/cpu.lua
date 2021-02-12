cpu = {}

local ffi = require("ffi")

pcall(ffi.load, 'native-lib', true)

ffi.cdef([[
    uint32_t symemu_cpu_get_reg(const int idx);
    uint32_t symemu_cpu_get_cpsr();
    uint32_t symemu_cpu_get_pc();
    uint32_t symemu_cpu_get_sp();
    uint32_t symemu_cpu_get_lr();

    void symemu_cpu_set_reg(const int idx, uint32_t value);
]])

function cpu.getReg(idx)
    return ffi.C.symemu_cpu_get_reg(idx)
end

function cpu.getCpsr(idx, val)
    return ffi.C.symemu_cpu_get_cpsr()
end

function cpu.getPc()
    return ffi.C.symemu_cpu_get_pc()
end

function cpu.getSp()
    return ffi.C.symemu_cpu_get_sp()
end

function cpu.getLr()
    return ffi.C.symemu_cpu_get_lr()
end

function cpu.setReg(idx, val)
    return ffi.C.symemu_cpu_set_reg(idx, val)
end

return cpu