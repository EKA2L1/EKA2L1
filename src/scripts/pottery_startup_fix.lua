local cpu = require('eka2l1.cpu')
local events = require('eka2l1.events')
local mem = require('eka2l1.mem')

local function clearDeviceCloseFlag()
    -- CIrrDeviceSymbian never initializes its Close flag, so run() reports the device as
    -- closed whenever the reused heap cell holds a nonzero byte there and nothing is drawn.
    mem.writeByte(cpu.getReg(0) + 0xB4, 0)
end

events.registerBreakpointHook('pottery.exe', 0x187410, 0, 0x20042DDE,
    clearDeviceCloseFlag, 0x58A29E91)
