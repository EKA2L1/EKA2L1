common = {}

local ffi = require("ffi")
ffi.cdef([[
    void symemu_log(const char *line);
]])

function common.log(str)
	local target = ffi.new("char[?]", #str)
	ffi.copy(target, str)

    ffi.C.symemu_log(target)
end

return common