events = {}

local ffi = require("ffi")
ffi.cdef([[
    typedef void (__stdcall *breakpoint_hit_lua_func)();

    void symemu_cpu_register_lib_hook(const char *lib_name, const uint32_t ord, const uint32_t process_uid, breakpoint_hit_lua_func func);
    void symemu_cpu_register_bkpt_hook(const char *image_name, const uint32_t addr, const uint32_t process_uid, breakpoint_hit_lua_func func);
]])

function events.registerLibraryInvoke(libName, ord, processUid, func)
    local libNameInC = ffi.new("char[?]", #libName)
    ffi.copy(libNameInC, libName)

    ffi.C.symemu_cpu_register_lib_hook(libNameInC, ord, processUid, func)
end

function events.registerBreakpointInvoke(libName, addr, processUid, func)
    local libNameInC = ffi.new("char[?]", #libName)
    ffi.copy(libNameInC, libName)

    ffi.C.symemu_cpu_register_bkpt_hook(libNameInC, addr, processUid, func)
end

return events