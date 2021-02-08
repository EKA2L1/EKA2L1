kernel = {
}

local ffi = require('ffi')
ffi.cdef([[
    typedef struct codeseg codeseg;
    typedef struct process process;

    codeseg *symemu_load_codeseg(const char *path);
    uint32_t symemu_codeseg_lookup(codeseg *seg, process *pr, const uint32_t ord);
    uint32_t symemu_codeseg_code_run_address(codeseg *seg, process *pr);
    uint32_t symemu_codeseg_data_run_address(codeseg *seg, process *pr);
    uint32_t symemu_codeseg_bss_run_address(codeseg *seg, process *pr);
    uint32_t symemu_codeseg_code_size(codeseg *seg);
    uint32_t symemu_codeseg_data_size(codeseg *seg);
    uint32_t symemu_codeseg_bss_size(codeseg *seg);
    uint32_t symemu_codeseg_export_count(codeseg *seg);
]])

-- Code segment object implementation
local codeseg = {}

function codeseg:new(o)
    o = o or {}   -- create object if user does not provide one
    setmetatable(o, self)
    self.__index = self
    return o
end

function codeseg:codeSize()
    return ffi.C.symemu_codeseg_code_size(self.impl)
end

function codeseg:dataSize()
    return ffi.C.symemu_codeseg_data_size(self.impl)
end

function codeseg:bssSize()
    return ffi.C.symemu_codeseg_bss_size(self.impl)
end

function codeseg:exportCount()
    return ffi.C.symemu_codeseg_export_count(self.impl)
end

function kernel.loadCodeseg(path)
	local target = ffi.new("char[?]", #path)
	ffi.copy(target, path)

    local res = ffi.C.symemu_load_codeseg(path)
    if res == nil then
        return nil
	end

    return codeseg:new{ impl = res }
end

-- End code segment object implementation

return kernel