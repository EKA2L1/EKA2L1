local cpu = require('eka2l1.cpu')
local common = require('eka2l1.common')
local events = require('eka2l1.events')

function WH40K_AdjustLoadingActiveObjPriority()
	cpu.setReg(0, 0x14)
end

common.log('Script enabled: Warhammer 40K slow loading fix.')
common.log('Loading active object got starved because screen update and audio runs at a fast rate under morden CPU')

events.registerBreakpointHook('6r92.app', 0x100DB3F8, 0, 0x101FD427, WH40K_AdjustLoadingActiveObjPriority)