kernel = {
}

local ffi = require('ffi')
ffi.cdef([[
    void free(void *ptr);

    typedef struct codeseg codeseg;
    typedef struct process process;
    typedef struct thread thread;

    codeseg *symemu_load_codeseg(const char *path);
    uint32_t symemu_codeseg_lookup(codeseg *seg, process *pr, const uint32_t ord);
    uint32_t symemu_codeseg_code_run_address(codeseg *seg, process *pr);
    uint32_t symemu_codeseg_data_run_address(codeseg *seg, process *pr);
    uint32_t symemu_codeseg_bss_run_address(codeseg *seg, process *pr);
    uint32_t symemu_codeseg_code_size(codeseg *seg);
    uint32_t symemu_codeseg_data_size(codeseg *seg);
    uint32_t symemu_codeseg_bss_size(codeseg *seg);
    uint32_t symemu_codeseg_export_count(codeseg *seg);
    
    thread *symemu_get_current_thread();
    uint32_t symemu_thread_stack_base(thread *thr);
    uint32_t symemu_thread_get_heap_base(thread *thr);
    uint32_t symemu_thread_get_register(thread *thr, uint8_t index);
    uint32_t symemu_thread_get_pc(thread *thr);
    uint32_t symemu_thread_get_lr(thread *thr);
    uint32_t symemu_thread_get_sp(thread *thr);
    uint32_t symemu_thread_get_cpsr(thread *thr);
    int symemu_thread_get_exit_reason(thread *thr);
    int symemu_thread_current_state(thread *thr);
    int symemu_thread_priority(thread *thr);
    const char *symemu_thread_name(thread *thr);
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

-- Begin thread object implementation
local thread = {}

function thread:new(o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

function thread:name()
    local cname = ffi.C.symemu_thread_name(self.impl)
    ffi.gc(cname, ffi.C.free)
    return ffi.string(cname)
end

function thread:stackBase()
    return ffi.C.symemu_thread_stack_base(self.impl)
end

function thread:heapBase()
    return ffi.C.symemu_thread_get_heap_base(self.impl)
end

function thread:getRegister(idx)
    return ffi.C.symemu_thread_get_register(self.impl, idx)
end

function thread:getPc()
    return ffi.C.symemu_thread_get_pc(self.impl, idx)
end

function thread:getLr()
    return ffi.C.symemu_thread_get_lr(self.impl, idx)
end

function thread:getSp()
    return ffi.C.symemu_thread_get_sp(self.impl, idx)
end

function thread:getCpsr()
    return ffi.C.symemu_thread_get_cpsr(self.impl, idx)
end

function thread:exitReason()
    return ffi.C.symemu_thread_get_exit_reason(self.impl)
end

function thread:currentState()
    return ffi.C.symemu_thread_current_state(self.impl)
end

function thread:priority()
    return ffi.C.symemu_thread_priority(self.impl)
end

function kernel.getCurrentThread()
    local thrimpl = ffi.C.symemu_get_current_thread()
    return thread:new{ impl = thrimpl }
end
    
-- End thread object implementation

return kernel