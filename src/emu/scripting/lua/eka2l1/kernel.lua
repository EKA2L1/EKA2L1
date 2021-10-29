kernel = {
}

local helper = require('eka2l1.helper')
local ffi = require('ffi')

ffi.cdef([[
    typedef struct codeseg codeseg;
    typedef struct process process;
    typedef struct thread thread;
    typedef struct server server;
    typedef struct session session;
    typedef struct ipc_msg ipc_msg;

    void eka2l1_free_codeseg(codeseg* seg);
    void eka2l1_free_process(process *pr);
    void eka2l1_free_thread(thread *thr);
    void eka2l1_free_server(server *svr);
    void eka2l1_free_session(session *ss);
    void eka2l1_free_ipc_msg(ipc_msg *msg);
	
    void eka2l1_free_string(const char* str);

    codeseg *eka2l1_load_codeseg(const char *path);
    uint32_t eka2l1_codeseg_lookup(codeseg *seg, process *pr, const uint32_t ord);
    uint32_t eka2l1_codeseg_code_run_address(codeseg *seg, process *pr);
    uint32_t eka2l1_codeseg_data_run_address(codeseg *seg, process *pr);
    uint32_t eka2l1_codeseg_bss_run_address(codeseg *seg, process *pr);
    uint32_t eka2l1_codeseg_code_size(codeseg *seg);
    uint32_t eka2l1_codeseg_data_size(codeseg *seg);
    uint32_t eka2l1_codeseg_bss_size(codeseg *seg);
    uint32_t eka2l1_codeseg_export_count(codeseg *seg);

    int32_t eka2l1_queries_all_processes(process ***pr);
    process *eka2l1_get_current_process();
    const char *eka2l1_process_read_memory(process *pr, uint32_t addr, uint32_t size);
    int eka2l1_process_write_memory(process *pr, uint32_t addr, const char *buf, uint32_t size);
    uint8_t eka2l1_process_read_byte(process *pr, uint32_t addr);
    uint16_t eka2l1_process_read_word(process *pr, uint32_t addr);
    uint32_t eka2l1_process_read_dword(process *pr, uint32_t addr);
    uint64_t eka2l1_process_read_qword(process *pr, uint32_t addr);
    thread *eka2l1_process_first_thread(process *pr);
    const char *eka2l1_process_executable_path(process *pr);
    const char *eka2l1_process_name(process *pr);

    thread *eka2l1_get_current_thread();
    thread *eka2l1_next_thread_in_process(thread *thr);
    process *eka2l1_thread_own_process(thread *thr);
    uint32_t eka2l1_thread_stack_base(thread *thr);
    uint32_t eka2l1_thread_get_heap_base(thread *thr);
    uint32_t eka2l1_thread_get_register(thread *thr, uint8_t index);
    uint32_t eka2l1_thread_get_pc(thread *thr);
    uint32_t eka2l1_thread_get_lr(thread *thr);
    uint32_t eka2l1_thread_get_sp(thread *thr);
    uint32_t eka2l1_thread_get_cpsr(thread *thr);
    int eka2l1_thread_get_exit_reason(thread *thr);
    int eka2l1_thread_current_state(thread *thr);
    int eka2l1_thread_priority(thread *thr);
    const char *eka2l1_thread_name(thread *thr);

    const char *eka2l1_server_name(server *svr);

    session *eka2l1_session_from_handle(uint32_t handle);
    server *eka2l1_session_server(session *ss);
    
    ipc_msg *eka2l1_ipc_message_from_handle(int guest_handle);
    int eka2l1_ipc_message_function(ipc_msg *msg);
    uint32_t eka2l1_ipc_message_arg(ipc_msg *msg, const int idx);
    uint32_t eka2l1_ipc_message_flags(ipc_msg *msg);
    thread *eka2l1_ipc_message_sender(ipc_msg *msg);
    session *eka2l1_ipc_message_session_wrapper(ipc_msg *msg);
    uint32_t eka2l1_ipc_message_request_status_address(ipc_msg *msg);
]])

handle = helper.class(function(self, impl)
    self.impl = impl
end)

-- Code segment object implementation
kernel.codeseg = helper.class(handle, function(self, impl)
    handle.init(self, impl)
    ffi.gc(self.impl, ffi.C.eka2l1_free_codeseg)
end)

function kernel.codeseg:codeSize()
    return ffi.C.eka2l1_codeseg_code_size(self.impl)
end

function kernel.codeseg:dataSize()
    return ffi.C.eka2l1_codeseg_data_size(self.impl)
end

function kernel.codeseg:bssSize()
    return ffi.C.eka2l1_codeseg_bss_size(self.impl)
end

function kernel.codeseg:exportCount()
    return ffi.C.eka2l1_codeseg_export_count(self.impl)
end

function kernel.loadCodeseg(path)
	local target = ffi.new("char[?]", #path + 1, path)

    local res = ffi.C.eka2l1_load_codeseg(path)
    if res == nil then
        return nil
	end

    return kernel.codeseg(res)
end

-- End code segment object implementation

-- Begin process object implementation
kernel.process = helper.class(handle, function(self, impl)
    handle.init(self, impl)
    ffi.gc(self.impl, ffi.C.eka2l1_free_process)
end)

function kernel.process:readByte(addr)
    return ffi.C.eka2l1_process_read_byte(self.impl, addr)
end

function kernel.process:readWord(addr)
    return ffi.C.eka2l1_process_read_word(self.impl, addr)
end

function kernel.process:readDword(addr)
    return ffi.C.eka2l1_process_read_dword(self.impl, addr)
end

function kernel.process:readQword(addr)
    return ffi.C.eka2l1_process_read_qword(self.impl, addr)
end

function kernel.process:name()
    local res = ffi.C.eka2l1_process_name(self.impl)
    local ret = ffi.string(res)

    ffi.C.eka2l1_free_string(ffi.gc(res, nil))
    return ret
end

function kernel.process:executablePath()
    local res = ffi.C.eka2l1_process_executable_path(self.impl)
    local ret = ffi.string(res)

    ffi.C.eka2l1_free_string(ffi.gc(res, nil))
    return ret
end

function kernel.process:readMemory(addr, size)
    local res = ffi.C.eka2l1_process_read_memory(self.impl, addr, size)
    ffi.gc(res, ffi.C.eka2l1_free_string)

    return res
end

function kernel.process:writeMemory(addr, buffer)
    local cbuf = ffi.new('char[?]', #buffer + 1, buffer)

    local res = ffi.C.eka2l1_process_write_memory(addr, cbuf, #buffer)
    if res == 0 then
        error('Fail to write memory, address is invalid!')
    end
end

function kernel.getCurrentProcess()
    local primpl = ffi.C.eka2l1_get_current_process()
    return kernel.process(primpl)
end

function kernel.getAllProcesses()
    local arr = ffi.new('process**[1]', nil)
    local count = ffi.C.eka2l1_queries_all_processes(arr)

    if count <= 0 then
        ffi.C.free(ffi.gc(arr, nil))
        return {}
    end

    local retarr = {}

    for i=1, count do
        retarr[i] = kernel.process(arr[0][i - 1])
    end
    
    ffi.C.free(ffi.gc(arr[0], nil))
    return retarr
end
-- End process object implementation

-- Begin thread object implementation
kernel.thread = helper.class(handle, function(self, impl)
    handle.init(self, impl)
    ffi.gc(self.impl, ffi.C.eka2l1_free_thread)
end)

function kernel.thread:name()
    local cname = ffi.C.eka2l1_thread_name(self.impl)
    local namer = ffi.string(cname)

    ffi.C.eka2l1_free_string(ffi.gc(cname, nil))

    return namer
end

function kernel.thread:stackBase()
    return ffi.C.eka2l1_thread_stack_base(self.impl)
end

function kernel.thread:heapBase()
    return ffi.C.eka2l1_thread_get_heap_base(self.impl)
end

function kernel.thread:getRegister(idx)
    return ffi.C.eka2l1_thread_get_register(self.impl, idx)
end

function kernel.thread:getPc()
    return ffi.C.eka2l1_thread_get_pc(self.impl, idx)
end

function kernel.thread:getLr()
    return ffi.C.eka2l1_thread_get_lr(self.impl, idx)
end

function kernel.thread:getSp()
    return ffi.C.eka2l1_thread_get_sp(self.impl, idx)
end

function kernel.thread:getCpsr()
    return ffi.C.eka2l1_thread_get_cpsr(self.impl, idx)
end

function kernel.thread:exitReason()
    return ffi.C.eka2l1_thread_get_exit_reason(self.impl)
end

function kernel.thread:currentState()
    return ffi.C.eka2l1_thread_current_state(self.impl)
end

function kernel.thread:priority()
    return ffi.C.eka2l1_thread_priority(self.impl)
end

function kernel.thread:nextInProcess()
    local thrimpl = ffi.C.eka2l1_next_thread_in_process(self.impl)
    if thrimpl == nil then
        return nil
    end

    return kernel.thread(thrimpl)
end

function kernel.thread:ownProcess()
    local primpl = ffi.C.eka2l1_thread_own_process(self.impl)
    if primpl == nil then
        return nil
    end

    return kernel.process(primpl)
end

function kernel.process:firstThread()
    local thrimpl = ffi.C.eka2l1_process_first_thread(self.impl)
    return kernel.thread(thrimpl)
end

function kernel.getCurrentThread()
    local thrimpl = ffi.C.eka2l1_get_current_thread()
    return kernel.thread(thrimpl)
end
    
-- End thread object implementation

-- Begin server object implementation
kernel.server = helper.class(handle, function(self, impl)
    handle.init(self, impl)
    ffi.gc(self.impl, ffi.C.eka2l1_free_server)
end)

function kernel.server:name()
    local namec = ffi.C.eka2l1_server_name(self.impl)
    local namer = ffi.string(namec)

    ffi.C.eka2l1_free_string(ffi.gc(namec, nil))

    return namer
end
-- End server object implementation

-- Begin session object implementation
kernel.session = helper.class(handle, function(self, impl)
    handle.init(self, impl)
    ffi.gc(self.impl, ffi.C.eka2l1_free_session)
end)

function kernel.session:server()
    local svimpl = ffi.C.eka2l1_session_server(self.impl)
    return kernel.server(svimpl)
end

function kernel.sessionFromHandle(handle)
    local ssimpl = ffi.C.eka2l1_session_from_handle(handle)
    if ssimpl == nil then
        return nil
    end

    return kernel.session(ssimpl)
end
-- End session object implementation

-- Begin IPC message implementation
kernel.ipcMessage = helper.class(handle, function(self, impl)
    handle.init(self, impl)
    ffi.gc(self.impl, ffi.C.eka2l1_free_ipc_msg)
end)

function kernel.ipcMessage:func()
    return ffi.C.eka2l1_ipc_message_function(self.impl)
end

function kernel.ipcMessage:flags()
    return ffi.C.eka2l1_ipc_message_flags(self.impl)
end

function kernel.ipcMessage:arg(idx)
    return ffi.C.eka2l1_ipc_message_arg(self.impl, idx)
end

function kernel.ipcMessage:sender()
    local thrimpl = ffi.C.eka2l1_ipc_message_sender(self.impl)
    return kernel.thread(thrimpl)
end

function kernel.ipcMessage:session()
    local ssimpl = ffi.C.eka2l1_ipc_message_session_wrapper(self.impl)
    return kernel.session(ssimpl)
end

function kernel.ipcMessage:requestStatusAddress()
    return ffi.C.eka2l1_ipc_message_request_status_address(self.impl)
end
-- End IPC message implementation

return kernel