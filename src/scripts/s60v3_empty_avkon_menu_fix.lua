local cpu = require('eka2l1.cpu')
local events = require('eka2l1.events')
local mem = require('eka2l1.mem')

local function skipEmptyAvkonMenu(cleanupOffset)
    -- R1 is Count()-1. LuaJIT exposes uint32_t as an unsigned number, so test
    -- the sign bit through its numeric range.
    if tonumber(cpu.getReg(1)) >= 0x80000000 then
        cpu.setReg(7, 0)
        cpu.setReg(15, cpu.getPc() + cleanupOffset)
    end
end

local function skipRm409EmptyAvkonMenu()
    skipEmptyAvkonMenu(0xF4)
end

local function skipRm320EmptyAvkonMenu()
    skipEmptyAvkonMenu(0x12C)
end

-- An empty menu must not expose a CBA and interrupt direct screen access.
local function deferRm92MenuCba()
    -- Positioning scratch is unused until after menu validation.
    mem.writeDword(cpu.getReg(13) + 0xC, cpu.getReg(6))
    cpu.setReg(15, cpu.getPc() + 0x14)
end

local function skipRm92EmptyAvkonMenu()
    skipEmptyAvkonMenu(0xD2)
end

local function createRm92MenuCba()
    if tonumber(cpu.getReg(0)) ~= 0 then
        mem.writeDword(cpu.getReg(13), 1)
        cpu.setReg(0, 3)
        cpu.setReg(1, 1)
        cpu.setReg(2, cpu.getReg(4) + 0x58)
        cpu.setReg(3, mem.readDword(cpu.getReg(13) + 0xC))
        cpu.setReg(15, cpu.getPc() - 0x118)
    end
end

local function finishRm92MenuCba()
    mem.writeDword(cpu.getReg(4) + 0x64, cpu.getReg(0))
    cpu.setReg(15, cpu.getPc() + 0x134)
end

events.registerRomExportBreakpointHook('eikcoctl.dll', 70, 0x39A36149,
    0x182, 0, 0x1000489E, skipRm409EmptyAvkonMenu)
events.registerRomExportBreakpointHook('eikcoctl.dll', 70, 0x1595EE13,
    0x17E, 0, 0x1000489E, skipRm320EmptyAvkonMenu)
events.registerRomExportBreakpointHook('eikcoctl.dll', 70, 0x5903C871,
    0x116, 0, 0x1000489E, deferRm92MenuCba)
events.registerRomExportBreakpointHook('eikcoctl.dll', 70, 0x5903C871,
    0x16C, 0, 0x1000489E, skipRm92EmptyAvkonMenu)
events.registerRomExportBreakpointHook('eikcoctl.dll', 70, 0x5903C871,
    0x23C, 0, 0x1000489E, createRm92MenuCba)
events.registerRomExportBreakpointHook('eikcoctl.dll', 70, 0x5903C871,
    0x128, 0, 0x1000489E, finishRm92MenuCba)
