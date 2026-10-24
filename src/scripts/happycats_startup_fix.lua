local cpu = require('eka2l1.cpu')
local events = require('eka2l1.events')
local mem = require('eka2l1.mem')

local function initFocusFlag()
    -- The game widget leaves its sounds and focus flags uninitialized until a 3s splash timer.
    -- A focus gain before that resumes music on a garbage QSound unless +0x3B happens to be nonzero.
    mem.writeByte(cpu.getReg(9) + 0x3B, 1)
end

events.registerBreakpointHook('happycats.exe', 0xB24F, 0, 0x200394B9,
    initFocusFlag, 0xD9FD7A4C)
