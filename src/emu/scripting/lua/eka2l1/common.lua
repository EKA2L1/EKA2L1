common = {}

local ffi = require("ffi")

pcall(ffi.load, 'native-lib', true)

ffi.cdef([[
    void eka2l1_log(const char *line);
]])

function common.log(str)
	local target = ffi.new("char[?]", #str)
	ffi.copy(target, str)

    ffi.C.eka2l1_log(target)
end

return common