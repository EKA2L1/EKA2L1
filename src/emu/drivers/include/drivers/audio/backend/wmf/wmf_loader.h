// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>

#include <propsys.h>

namespace loader {
    template <typename T>
    struct symbol {
        symbol() = default;
        symbol(HMODULE dll, const char* name) {
            if (dll) {
                ptr_symbol = reinterpret_cast<T*>(GetProcAddress(dll, name));
            }
        }

        operator T*() const {
            return ptr_symbol;
        }

        explicit operator bool() const {
            return ptr_symbol != nullptr;
        }

        T* ptr_symbol = nullptr;
    };

    extern symbol<HRESULT(ULONG, DWORD)> MFStartup;
    extern symbol<HRESULT(void)> MFShutdown;
    extern symbol<HRESULT(IUnknown*)> MFShutdownObject;
    extern symbol<HRESULT(DWORD, DWORD, IMFMediaBuffer**)> MFCreateAlignedMemoryBuffer;
    extern symbol<HRESULT(IMFSample**)> MFCreateSample;
    extern symbol<HRESULT(GUID, UINT32, const MFT_REGISTER_TYPE_INFO*, const MFT_REGISTER_TYPE_INFO*, IMFActivate***, UINT32*)> MFTEnumEx;
    extern symbol<HRESULT(IMFMediaType**)> MFCreateMediaType;
    extern symbol<HRESULT(LPCWSTR, IMFAttributes*, IMFSourceReader**)> MFCreateSourceReaderFromURL;
    extern symbol<HRESULT(LPCWSTR, IMFByteStream*, IMFAttributes*, IMFSinkWriter**)> MFCreateSinkWriterFromURL;
    extern symbol<HRESULT(REFGUID, DWORD, IMFAttributes*, IMFCollection**)> MFTranscodeGetAudioOutputAvailableTypes;
    extern symbol<HRESULT(IStream*, IMFByteStream**)> MFCreateMFByteStreamOnStream;
    extern symbol<HRESULT(IMFByteStream*, IMFAttributes*, IMFSourceReader**)> MFCreateSourceReaderFromByteStream;
    
    bool init_mf();
}