local cpu = require('eka2l1.cpu')
local events = require('eka2l1.events')
local mem = require('eka2l1.mem')

local function fixMansionBossPackName()
    -- The Mansion boss loads its cannon from pack "/sb", but data.pkg names it "sb". The failed
    -- lookup returns NULL and the pack reader dereferences it, which faults on the phone too.
    local name = cpu.getReg(1)
    if mem.readByte(name) == 0x2F then
        cpu.setReg(1, name + 1)
    end
end

events.registerBreakpointHook('bbd2_0x20024078.exe', 0x22165, 0, 0x20024078,
    fixMansionBossPackName, 0xD29BB435)
