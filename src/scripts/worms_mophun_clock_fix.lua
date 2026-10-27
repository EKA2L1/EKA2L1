local events = require('eka2l1.events')
local kernel = require('eka2l1.kernel')

local function maskClock()
    kernel.getCurrentProcess():setTickCountMask(0xFFFFFFFE)
end

events.registerBreakpointHook('mophun.dll', 0x10011B74, 0x101FD3F3, 0,
    maskClock, 0x086E4D44)
