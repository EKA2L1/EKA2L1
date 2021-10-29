--- **Common** contains utilities that is frequently used by the emulator.
-- @module common

common = {}

local ffi = require("ffi")

pcall(ffi.load, 'native-lib', true)

ffi.cdef([[
    void eka2l1_log(const char *line);
]])

--- Print a string into the emulator's log sinks.
---
--- The severity of the log will be **INFO**, and the category is **Scripting**
-- @param str The string to be logged out
function common.log(str)
	local target = ffi.new("char[?]", #str + 1, str)
    ffi.C.eka2l1_log(target)
end

return common