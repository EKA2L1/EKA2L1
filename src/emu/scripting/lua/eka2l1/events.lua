local ipcCtx = require('eka2l1.ipc.context')
local kern = require('eka2l1.kernel')
local common = require('eka2l1.common')

events = {}

events.EVENT_IPC_SEND = 0
events.EVENT_IPC_COMPLETE = 2
events.INVALID_HOOK_HANDLE = 0xFFFFFFFF

local ffi = require("ffi")

pcall(ffi.load, 'native-lib', true)

ffi.cdef([[
    void free(void *ptr);

    typedef struct ipc_msg ipc_msg;
    typedef struct thread thread;

    typedef void (__stdcall *breakpoint_hit_lua_func)();
    typedef void (__stdcall *ipc_sent_lua_func)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, thread*);
    typedef void (__stdcall *ipc_completed_lua_func)(ipc_msg*);

    uint32_t eka2l1_cpu_register_lib_hook(const char *lib_name, const uint32_t ord, const uint32_t process_uid, breakpoint_hit_lua_func func);
    uint32_t eka2l1_cpu_register_bkpt_hook(const char *image_name, const uint32_t addr, const uint32_t process_uid, breakpoint_hit_lua_func func);
    uint32_t eka2l1_register_ipc_sent_hook(const char *server_name, const int opcode, ipc_sent_lua_func func);
    uint32_t eka2l1_register_ipc_completed_hook(const char *server_name, const int opcode, ipc_completed_lua_func func);

    void eka2l1_clear_hook(const uint32_t hook_handle);
]])

function events.registerLibraryHook(libName, ord, processUid, func)
    local libNameInC = ffi.new("char[?]", #libName + 1, libName)

    return ffi.C.eka2l1_cpu_register_lib_hook(libNameInC, ord, processUid, function ()
        local ran, errorMsg = pcall(func)
        if not ran then
            common.log('Error running breakpoint script, ' .. errorMsg)
        end
    end)
end

function events.registerBreakpointHook(libName, addr, processUid, func)
    local libNameInC = ffi.new("char[?]", #libName + 1, libName)

    return ffi.C.eka2l1_cpu_register_bkpt_hook(libNameInC, addr, processUid, function ()
        local ran, errorMsg = pcall(func)
        if not ran then
            common.log('Error running breakpoint script, ' .. errorMsg)
        end
    end)
end

function events.registerIpcHook(serverName, opcode, when, func)
    local serverNameInC = ffi.new("char[?]", #serverName + 1, serverName)

    if when == events.EVENT_IPC_SEND then
        return ffi.C.eka2l1_register_ipc_sent_hook(serverNameInC, opcode, function (arg0, arg1, arg2, arg3, flags, reqsts, sender)
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
        return ffi.C.eka2l1_register_ipc_completed_hook(serverNameInC, opcode, function (msg)
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

function events.clearHook(handle)
    ffi.C.eka2l1_clear_hook(handle)
end

return events