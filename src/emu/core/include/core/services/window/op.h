#pragma once

enum WservMessages{
    EWservMessCommandBuffer,
    EWservMessShutdown,
    EWservMessInit,
    EWservMessFinish,
    EWservMessSyncMsgBuf,
    EWservMessAsynchronousService = 0x010000,
    EWservMessAnimDllAsyncCommand = 0x100000,
};