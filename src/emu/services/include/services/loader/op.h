#pragma once

enum TLoaderMsg {
    ELoadProcess = 1,
    ELoadLibrary = 2,
    ELoadLogicalDevice = 3,
    ELoadPhysicalDevice = 4,
    ELoadLocale = 5,
    ELoadFileSystem = 6,
    EGetInfo = 7,
    ELoaderDebugFunction = 8,
    ELoadFSExtension = 9,
    EGetInfoFromHeader = 10,
    ELoadFSPlugin = 11,
    ELoaderCancelLazyDllUnload = 12,
    ELdrDelete = 13,
    ECheckLibraryHash = 14,
    ELoadFSProxyDrive = 15,
    ELoadCodePage = 16,
    ELoaderRunReaper = 17,
    EMaxLoaderMsg
};