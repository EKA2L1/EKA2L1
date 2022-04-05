local cpu = require('eka2l1.cpu')
local common = require('eka2l1.common')
local events = require('eka2l1.events')
local strlib = require('string')

function WH40K_AdjustLoadingActiveObjPriority()
	cpu.setReg(0, 0x14)
end

function WH40K_SkipRecursiveWait()
	-- Recursive wait for reason that maybe is to extend the amount and the number of times waiting
	-- But it seems ineffective. It's better to just adjusting the time of wait
	cpu.setReg(15, cpu.getPc() + 24)
end
 
function WH40K_AdjustAudioFeedbackWaitTime()
	cpu.setReg(0, 5000)
end

common.log('Script enabled: Warhammer 40K slow loading fix.')
common.log('Loading active object got starved because screen update and audio runs at a fast rate under morden CPU')

events.registerBreakpointHook('6r92.app', 0x100DB3F8, 0, 0x101FD427, WH40K_AdjustLoadingActiveObjPriority)
events.registerBreakpointHook('6r92.app', 0x1007290C, 0, 0x101FD427, WH40K_SkipRecursiveWait)
events.registerBreakpointHook('6r92.app', 0x100728DC, 0, 0x101FD427, WH40K_AdjustAudioFeedbackWaitTime)