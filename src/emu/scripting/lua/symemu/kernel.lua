kernel = {
}

local ffi = require('ffi')
ffi.cdef([[
    void free(void *ptr);

    typedef struct codeseg codeseg;
    typedef struct process process;
    typedef struct thread thread;
    typedef struct server server;
    typedef struct session session;
    typedef struct ipc_msg ipc_msg;

    codeseg *symemu_load_codeseg(const char *path);
    uint32_t symemu_codeseg_lookup(codeseg *seg, process *pr, const uint32_t ord);
    uint32_t symemu_codeseg_code_run_address(codeseg *seg, process *pr);
    uint32_t symemu_codeseg_data_run_address(codeseg *seg, process *pr);
    uint32_t symemu_codeseg_bss_run_address(codeseg *seg, process *pr);
    uint32_t symemu_codeseg_code_size(codeseg *seg);
    uint32_t symemu_codeseg_data_size(codeseg *seg);
    uint32_t symemu_codeseg_bss_size(codeseg *seg);
    uint32_t symemu_codeseg_export_count(codeseg *seg);

    int32_t symemu_queries_all_processes(process ***pr);
    process *symemu_get_current_process();
    const char *symemu_process_read_memory(process *pr, uint32_t addr, uint32_t size);
    int symemu_process_write_memory(process *pr, uint32_t addr, const char *buf, uint32_t size);
    uint8_t symemu_process_read_byte(process *pr, uint32_t addr);
    uint16_t symemu_process_read_word(process *pr, uint32_t addr);
    uint32_t symemu_process_read_dword(process *pr, uint32_t addr);
    uint64_t symemu_process_read_qword(process *pr, uint32_t addr);
    thread *symemu_process_first_thread(process *pr);

    thread *symemu_get_current_thread();
    thread *symemu_next_thread_in_process(thread *thr);
    process *symemu_thread_own_process(thread *thr);
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

    const char *symemu_server_name(server *svr);

    session *symemu_session_from_handle(uint32_t handle);
    server *symemu_session_server(session *ss);
    
    ipc_msg *symemu_ipc_message_from_handle(int guest_handle);
    int symemu_ipc_message_function(ipc_msg *msg);
    uint32_t symemu_ipc_message_arg(ipc_msg *msg, const int idx);
    uint32_t symemu_ipc_message_flags(ipc_msg *msg);
    thread *symemu_ipc_message_sender(ipc_msg *msg);
    session *symemu_ipc_message_session_wrapper(ipc_msg *msg);
    uint32_t symemu_ipc_message_request_status_address(ipc_msg *msg);
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

    ffi.gc(res, ffi.C.free)
    return codeseg:new{ impl = res }
end

-- End code segment object implementation

-- Begin process object implementation
local process = {}

function process:new(o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

function process:readByte(addr)
    return ffi.C.symemu_process_read_byte(self.impl, addr)
end

function process:readWord(addr)
    return ffi.C.symemu_process_read_word(self.impl, addr)
end

function process:readDword(addr)
    return ffi.C.symemu_process_read_dword(self.impl, addr)
end

function process:readQword(addr)
    return ffi.C.symemu_process_read_qword(self.impl, addr)
end

function process:readMemory(addr, size)
    local res = ffi.C.symemu_process_read_memory(self.impl, addr, size)
    ffi.gc(res, ffi.C.free)

    return res
end

function process:writeMemory(addr, buffer)
    local cbuf = ffi.new('char[?]', #buffer)
    ffi.copy(cbuf, buffer)

    local res = ffi.C.symemu_process_write_memory(addr, cbuf, #buffer)
    if res == 0 then
        error('Fail to write memory, address is invalid!')
    end
end

function kernel.getCurrentProcess()
    local primpl = ffi.C.symemu_get_current_process()
    ffi.gc(primpl, ffi.C.free);

    return process:new{ impl = primpl }
end

function kernel.getAllProcesses()
    local arr = ffi.new('process**[1]', nil)
    local count = ffi.C.symemu_queries_all_processes(arr)

    if count <= 0 then
        ffi.C.free(ffi.gc(arr, nil))
        return {}
    end

    local retarr = {}

    for i=1, count do
        retarr[i] = process:new{ impl = arr[0][i - 1] }
        ffi.gc(retarr[i].impl, ffi.C.free)
    end
    
    ffi.C.free(ffi.gc(arr[0], nil))
    return retarr
end
-- End process object implementation

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

function thread:nextInProcess()
    local thrimpl = ffi.C.symemu_next_thread_in_process(self.impl)
    if thrimpl == nil then
        return nil
    end

    ffi.gc(thrimpl, ffi.C.free)
    return thread:new{ impl = thrimpl }
end

function thread:ownProcess()
    local primpl = ffi.C.symemu_thread_own_process(self.impl)
    if primpl == nil then
        return nil
    end

    ffi.gc(primpl, ffi.C.free)
    return process:new{ impl = primpl }
end

function process:firstThread()
    local thrimpl = ffi.C.symemu_process_first_thread(self.impl)
    ffi.gc(thrimpl, ffi.C.free)

    return thread:new{ impl = thrimpl }
end

function kernel.getCurrentThread()
    local thrimpl = ffi.C.symemu_get_current_thread()
    ffi.gc(thrimpl, ffi.C.free)

    return thread:new{ impl = thrimpl }
end

function kernel.makeThreadFromHandle(handle)
    ffi.gc(handle, ffi.C.free)
    return thread:new{ impl = handle }
end
    
-- End thread object implementation

-- Begin server object implementation
local server = {}

function server:new(o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

function server:name()
    local namec = ffi.C.symemu_server_name(self.impl)
    ffi.gc(namec, ffi.C.free)

    return ffi.string(namec)
end
-- End server object implementation

-- Begin session object implementation
local session = {}

function session:new(o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

function session:server()
    local svimpl = ffi.C.symemu_session_server(self.impl)
    ffi.gc(svimpl, ffi.C.free)

    return server:new{ impl = svimpl }
end

function kernel.sessionFromHandle(handle)
    local ssimpl = ffi.C.symemu_session_from_handle(handle)
    if ssimpl == nil then
        return nil
    end

    ffi.gc(ssimpl, ffi.C.free)
    return session:new{ impl = ssimpl }
end
-- End session object implementation

-- Begin IPC message implementation
local ipcMessage = {}

function ipcMessage:new(o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

function ipcMessage:func()
    return ffi.C.symemu_ipc_message_function(self.impl)
end

function ipcMessage:flags()
    return ffi.C.symemu_ipc_message_flags(self.impl)
end

function ipcMessage:arg(idx)
    return ffi.C.symemu_ipc_message_arg(self.impl, idx)
end

function ipcMessage:sender()
    local thrimpl = ffi.C.symemu_ipc_message_sender(self.impl)
    ffi.gc(thrimpl, ffi.C.free)

    return thread:new{ impl = thrimpl }
end

function ipcMessage:session()
    local ssimpl = ffi.C.symemu_ipc_message_session_wrapper(self.impl)
    ffi.gc(ssimpl, ffi.C.free)

    return session:new{ impl = ssimpl }
end

function ipcMessage:requestStatusAddress()
    return ffi.C.symemu_ipc_message_request_status_address(self.impl)
end

function kernel.makeMessageFromHandle(msg)
    ffi.gc(msg, ffi.C.free)
    return ipcMessage:new{ impl = msg }
end
-- End IPC message implementation

return kernel