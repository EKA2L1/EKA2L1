local kernel = require('symemu.kernel')
local ctx = require('symemu.ipc.context')
local common = require('symemu.common')
local events = require('symemu.events')

local PROCESS_NAME = "vbag2"
local THREAD_NAME = "Main"

function shouldCaptureThisThread(thr)
    if (thr:name() == THREAD_NAME) then
        return true
    end

    return false
end

-- TODO! This script will be completed in future :DD
function bufferToBeFilledHook(ctx)
	local targetThr = ctx:ownThread()

    if shouldCaptureThisThread(targetThr) == true then
        common.log('Another buffer to be filled...')
    end
end

events.registerIpcInvoke("!MMFDevServer", 19, EVENT_IPC_SEND, bufferToBeFilledHook)