
// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <drivers/audio/backend/wmf/wmf_loader.h>

#include <cstdint>
#include <memory>

#include <common/log.h>

namespace {
    struct library_deleter {
        using pointer = HMODULE;
        void operator()(HMODULE h) const {
            if (h != nullptr)
                FreeLibrary(h);
        }
    };

    std::unique_ptr<HMODULE, library_deleter> mf_dll{nullptr};
    std::unique_ptr<HMODULE, library_deleter> mfplat_dll{nullptr};
    std::unique_ptr<HMODULE, library_deleter> mfreadwrite_dll{nullptr};
}

namespace loader {
    symbol<HRESULT(ULONG, DWORD)> MFStartup;
    symbol<HRESULT(void)> MFShutdown;
    symbol<HRESULT(IUnknown*)> MFShutdownObject;
    symbol<HRESULT(DWORD, DWORD, IMFMediaBuffer**)> MFCreateAlignedMemoryBuffer;
    symbol<HRESULT(IMFSample**)> MFCreateSample;
    symbol<HRESULT(GUID, UINT32, const MFT_REGISTER_TYPE_INFO*, const MFT_REGISTER_TYPE_INFO*, IMFActivate***, UINT32*)> MFTEnumEx;
    symbol<HRESULT(IMFMediaType**)> MFCreateMediaType;
    symbol<HRESULT(LPCWSTR, IMFAttributes*, IMFSourceReader**)> MFCreateSourceReaderFromURL;
    symbol<HRESULT(LPCWSTR, IMFByteStream*, IMFAttributes*, IMFSinkWriter**)> MFCreateSinkWriterFromURL;
    symbol<HRESULT(REFGUID, DWORD, IMFAttributes*, IMFCollection**)> MFTranscodeGetAudioOutputAvailableTypes;
    symbol<HRESULT(IStream*, IMFByteStream**)> MFCreateMFByteStreamOnStream;
    symbol<HRESULT(IMFByteStream*, IMFAttributes*, IMFSourceReader**)> MFCreateSourceReaderFromByteStream;

    static void print_dll_load_error(const std::string &dll_name) {
        DWORD error_message_id = GetLastError();
        LPSTR message_buffer = nullptr;

        const std::size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error_message_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

        std::string message(message_buffer, size);

        LocalFree(message_buffer);
        LOG_ERROR(eka2l1::DRIVER_AUD, "Could not load {}: {}", dll_name, message);
    }

    bool init_mf() {
        mf_dll.reset(LoadLibrary(TEXT("mf.dll")));
        if (!mf_dll) {
            print_dll_load_error("mf.dll");
            return false;
        }

        mfplat_dll.reset(LoadLibrary(TEXT("mfplat.dll")));
        if (!mfplat_dll) {
            print_dll_load_error("mfplat.dll");
            return false;
        }

        mfreadwrite_dll.reset(LoadLibrary(TEXT("mfreadwrite.dll")));
        if (!mfplat_dll) {
            print_dll_load_error("mfreadwrite.dll");
            return false;
        }

        MFStartup = symbol<HRESULT(ULONG, DWORD)>(mfplat_dll.get(), "MFStartup");
        if (!MFStartup) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFStartup");
            return false;
        }

        MFShutdown = symbol<HRESULT(void)>(mfplat_dll.get(), "MFShutdown");
        if (!MFShutdown) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFShutdown");
            return false;
        }

        MFShutdownObject = symbol<HRESULT(IUnknown*)>(mf_dll.get(), "MFShutdownObject");
        if (!MFShutdownObject) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFShutdownObject");
            return false;
        }

        MFCreateAlignedMemoryBuffer = symbol<HRESULT(DWORD, DWORD, IMFMediaBuffer**)>(
            mfplat_dll.get(), "MFCreateAlignedMemoryBuffer");

        if (!MFCreateAlignedMemoryBuffer) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFCreateAlignedMemoryBuffer");
            return false;
        }

        MFCreateSample = symbol<HRESULT(IMFSample**)>(mfplat_dll.get(), "MFCreateSample");
        if (!MFCreateSample) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFCreateSample");
            return false;
        }

        MFTEnumEx = symbol<HRESULT(GUID, UINT32, const MFT_REGISTER_TYPE_INFO*, const MFT_REGISTER_TYPE_INFO*,
            IMFActivate***, UINT32*)>(mfplat_dll.get(), "MFTEnumEx");

        if (!MFTEnumEx) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFTEnumEx");
            return false;
        }

        MFCreateMediaType = symbol<HRESULT(IMFMediaType**)>(mfplat_dll.get(), "MFCreateMediaType");
        if (!MFCreateMediaType) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFCreateMediaType");
            return false;
        }

        MFCreateSourceReaderFromURL = symbol<HRESULT(LPCWSTR, IMFAttributes*, IMFSourceReader**)>(mfreadwrite_dll.get(), "MFCreateSourceReaderFromURL");
        if (!MFCreateSourceReaderFromURL) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFCreateSourceReaderFromURL");
            return false;
        }

        MFCreateSinkWriterFromURL = symbol<HRESULT(LPCWSTR, IMFByteStream*, IMFAttributes*, IMFSinkWriter**)>(mfreadwrite_dll.get(), "MFCreateSinkWriterFromURL");
        if (!MFCreateSinkWriterFromURL) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFCreateSinkWriterFromURL");
            return false;
        }
        
        MFTranscodeGetAudioOutputAvailableTypes = symbol<HRESULT(REFGUID, DWORD, IMFAttributes*, IMFCollection**)>(mf_dll.get(), "MFTranscodeGetAudioOutputAvailableTypes");
        if (!MFTranscodeGetAudioOutputAvailableTypes) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFTranscodeGetAudioOutputAvailableTypes");
            return false;
        }
        
        MFCreateMFByteStreamOnStream = symbol<HRESULT(IStream*, IMFByteStream**)>(mfplat_dll.get(), "MFCreateMFByteStreamOnStream");
        if (!MFCreateMFByteStreamOnStream) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFCreateMFByteStreamOnStream");
            return false;
        }

        MFCreateSourceReaderFromByteStream = symbol<HRESULT(IMFByteStream*, IMFAttributes*, IMFSourceReader**)>(mfreadwrite_dll.get(), "MFCreateSourceReaderFromByteStream");
        if (!MFCreateSourceReaderFromByteStream) {
            LOG_ERROR(eka2l1::DRIVER_AUD, "Cannot load function MFCreateSourceReaderFromByteStream");
            return false;
        }

        return true;
    }
}