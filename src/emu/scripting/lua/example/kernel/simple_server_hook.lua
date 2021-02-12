local kernel = require('symemu.kernel')
local events = require('symemu.events')
local ctx = require('symemu.ipc.context')
local strlib = require('string')

function fileServerConnectHook(ctx)
    local sd = ctx:ownThread()
    common.log(strlib.format('Request to connect to file server through thread %s', sd:name()))
end

events.registerIpcInvoke('FileServer', -1, EVENT_IPC_SEND, fileServerConnectHook)