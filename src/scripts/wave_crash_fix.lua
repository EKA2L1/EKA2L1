local cpu = require('eka2l1.cpu')
local events = require('eka2l1.events')

local function skipUnstartedAnimationRefresh()
    -- Starting a run refreshes a sprite animation that never started (frame -1).
    -- The game then reads 64KB past a heap cell, which only happens to be mapped on the phone.
    if cpu.getReg(1) >= 0x80000000 then
        cpu.setReg(15, cpu.getPc() + 0x16)
    end
end

local function skipZeroFirePeriod()
    -- Only type-0 enemy waves set their fire period, but every live wave is taken modulo it
    -- before the type check. Take the path that check picks anyway instead of dividing by zero.
    if cpu.getReg(1) == 0 then
        cpu.setReg(15, cpu.getPc() - 0x28)
    end
end

events.registerBreakpointHook('cboxapp_0x20041fae.exe', 0x956B, 0, 0x20041FAE,
    skipUnstartedAnimationRefresh, 0xCB7985E9)
events.registerBreakpointHook('cboxapp_0x20041fae.exe', 0x1D44F, 0, 0x20041FAE,
    skipZeroFirePeriod, 0xCB7985E9)
