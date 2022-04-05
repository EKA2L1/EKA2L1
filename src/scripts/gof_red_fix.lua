local common = require('eka2l1.common')
local evt = require('eka2l1.events')
local cpu = require('eka2l1.cpu')
local kern = require('eka2l1.kernel')
local strlib = require('string')

gofExecSeg = nil

function changeShipRedColour()
    if ((cpu.getReg(0) == 0x3F800000) and (cpu.getReg(1) == 0x3D888889) and (cpu.getReg(2) == 0x3D888889) and (cpu.getReg(3) == 0)) then
        cpu.setReg(0, 0x3F800000)
        cpu.setReg(1, 0x3F800000)
        cpu.setReg(2, 0x3F800000)
        cpu.setReg(3, 0x3F800000)
    end 
end

if (gofExecSeg == nil) then
    gofExecSeg = kern.loadCodeseg('gof.exe')
end

scriptEnabled = false

common.log(strlib.format('Codeseg size GOF %d', gofExecSeg:codeSize()))

if (gofExecSeg:codeSize() == 6007012) then 
    evt.registerBreakpointHook('gof.exe', 0x00153310, 0, 0x2002FEA0, changeShipRedColour)
end

if (scriptEnabled) then 
    common.log('Script enabled: Galaxy on Fire red ship hack fix')
end