local ipcCtx = require('symemu.ipc.context')
local kern = require('symemu.kernel')
local common = require('symemu.common')

events = {}

EVENT_IPC_SEND = 0
EVENT_IPC_COMPLETE = 2

local ffi = require("ffi")

pcall(ffi.load, 'native-lib', true)

ffi.cdef([[
    void free(void *ptr);

    typedef struct ipc_msg ipc_msg;
    typedef struct thread thread;

    typedef void (__stdcall *breakpoint_hit_lua_func)();
    typedef void (__stdcall *ipc_sent_lua_func)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, thread*);
    typedef void (__stdcall *ipc_completed_lua_func)(ipc_msg*);

    void symemu_cpu_register_lib_hook(const char *lib_name, const uint32_t ord, const uint32_t process_uid, breakpoint_hit_lua_func func);
    void symemu_cpu_register_bkpt_hook(const char *image_name, const uint32_t addr, const uint32_t process_uid, breakpoint_hit_lua_func func);
    void symemu_register_ipc_sent_hook(const char *server_name, const int opcode, ipc_sent_lua_func func);
    void symemu_register_ipc_completed_hook(const char *server_name, const int opcode, ipc_completed_lua_func func);
]])

function events.registerLibraryInvoke(libName, ord, processUid, func)
    local libNameInC = ffi.new("char[?]", #libName)
    ffi.copy(libNameInC, libName)

    ffi.C.symemu_cpu_register_lib_hook(libNameInC, ord, processUid, function ()
        local ran, errorMsg = pcall(func)
        if not ran then
            common.log('Error running breakpoint script, ' .. errorMsg)
        end
    end)
end

function events.registerBreakpointInvoke(libName, addr, processUid, func)
    local libNameInC = ffi.new("char[?]", #libName)
    ffi.copy(libNameInC, libName)

    ffi.C.symemu_cpu_register_bkpt_hook(libNameInC, addr, processUid, function ()
        local ran, errorMsg = pcall(func)
        if not ran then
            common.log('Error running breakpoint script, ' .. errorMsg)
        end
    end)
end

function events.registerIpcInvoke(serverName, opcode, when, func)
    local serverNameInC = ffi.new("char[?]", #serverName)
    ffi.copy(serverNameInC, serverName)

    if when == EVENT_IPC_SEND then
        ffi.C.symemu_register_ipc_sent_hook(serverNameInC, opcode, function (arg0, arg1, arg2, arg3, flags, reqsts, sender)
            local ran, retval = pcall(ipcCtx.makeFromValues, opcode, arg0, arg1, arg2, arg3, flags, reqsts, kern.makeThreadFromHandle(sender))
            if not ran then
                common.log('Fail to create IPC context, ' .. retval)
                return
            end

            ran, retval = pcall(func, retval)
            if not ran then
                common.log('IPC sent callback encountered error, '.. retval)
            end
        end)
    else
        ffi.C.symemu_register_ipc_completed_hook(serverNameInC, opcode, function (msg)
            local ran, retval = pcall(ipcCtx.makeFromMessage, kern.makeMessageFromHandle(msg))
            if not ran then
                common.log('Fail to create IPC context, ' .. retval)
                return
            end
            
            ran, retval = pcall(func, retval)
            if not ran then
                common.log('IPC sent callback encountered error, '.. retval)
            end
        end)
    end
end

return events