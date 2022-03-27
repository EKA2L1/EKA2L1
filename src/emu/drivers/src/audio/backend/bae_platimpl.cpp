/*
 * Copyright (c) 2021 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <BAE_Source/Platform/BAE_API.h>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <chrono>
#include <cstdio>
#include <thread>

#include <drivers/audio/stream.h>
#include <drivers/audio/audio.h>

#include <common/container.h>
#include <common/fileutils.h>
#include <common/platform.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <fcntl.h>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#include <io.h>
#else
#if EKA2L1_PLATFORM(ANDROID)
#include <common/android/storage.h>
#endif

#include <pthread.h>
#include <unistd.h>
#include <sys/uio.h>
#endif

static std::unique_ptr<eka2l1::drivers::audio_output_stream> global_baeout_stream;
static eka2l1::drivers::audio_driver *global_baedriver = nullptr;
static std::vector<std::int16_t> bae_11ms_buffer;
static eka2l1::common::ring_buffer<std::int16_t, 0x40000> bae_buffer_queue; 
static float global_originalvolume = -1.0f;

extern "C" {
void BAE_PrintHexDump(void *address, long length) {
}

void BAE_DisplayMemoryUsage(int detailLevel) {

}

int BAE_Setup() {
    return 0;
}

int BAE_Cleanup() {
    return 0;
}

void *BAE_Allocate(unsigned long size) {
    return calloc(1, size);
}

void BAE_Deallocate(void *memoryBlock) {
    free(memoryBlock);
}

unsigned long BAE_GetSizeOfMemoryUsed(void) {
    return 0;
}

unsigned long BAE_GetMaxSizeOfMemoryUsed(void) {
    return 0;
}

int BAE_IsBadReadPointer(void *memoryBlock, unsigned long size) {
    // Unsupported, so return 2
    return 2;
}

unsigned long BAE_SizeOfPointer(void *memoryBlock) {
    return 0;
}

void BAE_BlockMove(void *source, void *dest, unsigned long size) {
    memmove(dest, source, size);
}

int BAE_IsStereoSupported() {
    return true;
}

int BAE_Is8BitSupported() {
    return 0;
}

int BAE_Is16BitSupported() {
    return 1;
}

short int BAE_GetHardwareVolume(void) {
    if (global_originalvolume >= 0.0f) {
        return static_cast<short int>(global_originalvolume * 256);
    }

    if (!global_baeout_stream) {
        return 256;
    }

    return static_cast<short int>(global_baeout_stream->get_volume() * 256);
}

void BAE_SetHardwareVolume(short int theVolume) {
    if (global_originalvolume >= 0.0f) {
        global_originalvolume = theVolume / 256.0f;
    } else {
        if (global_baeout_stream) {
            global_baeout_stream->set_volume(static_cast<float>(theVolume) / 256.0f);
        }
    }
}

short int BAE_GetHardwareBalance(void) {
    return 0;
}

void BAE_SetHardwareBalance(short int balance) {
}

unsigned long BAE_Microseconds() {
    static std::uint64_t last_recorded = 0;
    static bool first_recorded = false;

    if (!first_recorded) {
        last_recorded = std::chrono::duration_cast<std::chrono::microseconds>(
           std::chrono::system_clock::now().time_since_epoch()).count();

        first_recorded = true;
    }

    std::uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
           std::chrono::system_clock::now().time_since_epoch()).count();

    return static_cast<unsigned long>(now - last_recorded);
}

void BAE_WaitMicroseconds(unsigned long wait) {
    std::this_thread::sleep_for(std::chrono::microseconds(wait));
}

void BAE_CopyFileNameNative(void *fileNameSource, void *fileNameDest) {
    char *dest = nullptr;
    const char *src = nullptr;

    if (fileNameSource && fileNameDest) {
        dest = reinterpret_cast<char*>(fileNameDest);
        src = reinterpret_cast<char*>(fileNameSource);
        if (src == NULL) {
            src = "";
        }
        if (dest) {
            while (*src) {
                *dest++ = *src++;
            }
            *dest = 0;
        }
    }
}

long BAE_FileCreate(void *fileName) {
    FILE *f = eka2l1::common::open_c_file(reinterpret_cast<const char*>(fileName), "wb");
    if (!f) {
        return -1;
    }

    fclose(f);
    return 0;
}

long BAE_FileDelete(void *fileName) {
    return (eka2l1::common::remove(reinterpret_cast<const char*>(fileName)) ? 0 : -1);
}

long BAE_FileOpenForRead(void *fileName) {
    const char *fname_casted = reinterpret_cast<const char*>(fileName);

#if EKA2L1_PLATFORM(WIN32)
    std::wstring name_unicode = eka2l1::common::utf8_to_wstr(fname_casted);
    return _wopen(name_unicode.c_str(), _O_RDONLY | _O_BINARY, _S_IREAD);
#elif EKA2L1_PLATFORM(POSIX)
#if EKA2L1_PLATFORM(ANDROID)
    if (eka2l1::is_content_uri(fname_casted)) {
        return eka2l1::common::android::open_content_uri_fd(fname_casted, eka2l1::common::android::open_content_uri_mode::READ);
    }
#endif

    return open(fname_casted, O_RDONLY);
#endif
}

long BAE_FileOpenForWrite(void *fileName) {
    const char *fname_casted = reinterpret_cast<const char*>(fileName);

#if EKA2L1_PLATFORM(WIN32)
    std::wstring name_unicode = eka2l1::common::utf8_to_wstr(fname_casted);
    return _wopen(name_unicode.c_str(), _O_WRONLY | _O_BINARY, _S_IWRITE);
#elif EKA2L1_PLATFORM(POSIX)
#if EKA2L1_PLATFORM(ANDROID)
    if (eka2l1::is_content_uri(fname_casted)) {
        return eka2l1::common::android::open_content_uri_fd(fname_casted, eka2l1::common::android::open_content_uri_mode::READ_WRITE);
    }
#endif

    return open(fname_casted, O_WRONLY | O_CREAT | O_TRUNC, 0600);
#endif
}

long BAE_FileOpenForReadWrite(void *fileName) {
    const char *fname_casted = reinterpret_cast<const char*>(fileName);

#if EKA2L1_PLATFORM(WIN32)
    std::wstring name_unicode = eka2l1::common::utf8_to_wstr(fname_casted);
    return _wopen(name_unicode.c_str(), _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
#elif EKA2L1_PLATFORM(POSIX)
#if EKA2L1_PLATFORM(ANDROID)
    if (eka2l1::is_content_uri(fname_casted)) {
        return eka2l1::common::android::open_content_uri_fd(fname_casted, eka2l1::common::android::open_content_uri_mode::READ_WRITE);
    }
#endif

    return open(fname_casted, O_RDWR);
#endif
}

void BAE_FileClose(long fileReference) {
#if EKA2L1_PLATFORM(WIN32)
    _close(fileReference);
#elif EKA2L1_PLATFORM(POSIX)
    close(fileReference);
#endif
}

long BAE_ReadFile(long fileReference, void *pBuffer, long bufferLength) {
#if EKA2L1_PLATFORM(WIN32)
    return _read(fileReference, pBuffer, bufferLength);
#elif EKA2L1_PLATFORM(POSIX)
    return read(fileReference, pBuffer, bufferLength);
#endif
}

// Write a block of memory from a file
// Return -1 if error, otherwise length of data written.
long BAE_WriteFile(long fileReference, void *pBuffer, long bufferLength) {
#if EKA2L1_PLATFORM(WIN32)
    return _write(fileReference, pBuffer, bufferLength);
#elif EKA2L1_PLATFORM(POSIX)
    return write(fileReference, pBuffer, bufferLength);
#endif
}

// set file position in absolute file byte position
long BAE_SetFilePosition(long fileReference, unsigned long filePosition) {
#if EKA2L1_PLATFORM(WIN32)
    return (_lseek(fileReference, filePosition, SEEK_SET) == -1) ? -1 : 0;
#elif EKA2L1_PLATFORM(POSIX)
    return (lseek(fileReference, filePosition, SEEK_SET) == -1) ? -1 : 0;
#endif
}

// get file position in absolute file bytes
unsigned long BAE_GetFilePosition(long fileReference) {
#if EKA2L1_PLATFORM(WIN32)
    return _lseek(fileReference, 0, SEEK_CUR);
#elif EKA2L1_PLATFORM(POSIX)
    return lseek(fileReference, 0, SEEK_CUR);
#endif
}

// get length of file
unsigned long BAE_GetFileLength(long fileReference) {
    unsigned long pos;

#if EKA2L1_PLATFORM(WIN32)
    pos = _lseek(fileReference, 0, SEEK_END);
    _lseek(fileReference, 0, SEEK_SET);
#elif EKA2L1_PLATFORM(POSIX)
    pos = lseek(fileReference, 0, SEEK_END);
    lseek(fileReference, 0, SEEK_SET);
#endif

    return pos;
}

// set the length of a file. Return 0, if ok, or -1 for error
int BAE_SetFileLength(long fileReference, unsigned long newSize) {
    return -1;
}

static std::size_t minibae_data_callback(std::int16_t *buffer, const std::size_t frame_count) {
    const std::size_t total_to_grab = global_baeout_stream->get_channels() * frame_count;

    if (global_baedriver && global_baedriver->suspending()) {
        std::memset(buffer, 0, total_to_grab * sizeof(std::int16_t));
        return frame_count;
    }

    auto value = BAE_GetMaxSamplePerSlice() * global_baeout_stream->get_channels();
    if (bae_11ms_buffer.size() < value) {
        bae_11ms_buffer.resize(value);
    }

    if ((bae_buffer_queue.size() == 0) && (total_to_grab == value)) {
        // Directly build and submit, no need to go through queue
        BAE_BuildMixerSlice(nullptr, buffer, static_cast<long>(total_to_grab * 2), static_cast<long>(frame_count));
        return frame_count;
    }

    while (bae_buffer_queue.size() < total_to_grab) {
        // Keep building until enough to grab some frames
        BAE_BuildMixerSlice(nullptr, bae_11ms_buffer.data(), static_cast<long>(total_to_grab * 2), static_cast<long>(frame_count));
        bae_buffer_queue.push(bae_11ms_buffer.data(), value);
    }

    std::vector<std::int16_t> received = bae_buffer_queue.pop(total_to_grab);
    std::memcpy(buffer, received.data(), received.size() * 2);

    if (received.size() < total_to_grab) {
        std::memset(buffer + received.size(), 0, total_to_grab - received.size());
    }
    return frame_count;
}

void BAE_SetActiveAudioDriver(eka2l1::drivers::audio_driver *driver) {
    if (global_baeout_stream && (driver != global_baedriver)) {
        global_baeout_stream->stop();
        global_baeout_stream = driver->new_output_stream(global_baeout_stream->get_sample_rate(),
            global_baeout_stream->get_channels(), minibae_data_callback);
        global_baeout_stream->start();
    }

    global_baedriver = driver;
}

void BAE_DriverDeactivated(eka2l1::drivers::audio_driver *driver) {
    if (global_baedriver == driver) {
        global_baeout_stream.reset();
    }

    global_baedriver = nullptr;
}

eka2l1::drivers::audio_driver *BAE_GetActiveAudioDriver() {
    return global_baedriver;
}

// return 0 if ok, -1 if failed
int BAE_AquireAudioCard(void *threadContext, unsigned long sampleRate, unsigned long channels, unsigned long bits) {
    if (bits != 16) {
        LOG_ERROR(eka2l1::DRIVER_AUD, "Unable to create non-16 bit stream (bitGiven={})", bits);
        return -1;
    }

    auto new_stream = global_baedriver->new_output_stream(sampleRate, static_cast<std::uint8_t>(channels), minibae_data_callback);
    if (!new_stream) {
        return -1;
    }

    global_baeout_stream = std::move(new_stream);
    global_baeout_stream->start();

    return 0;
}

// Release and free audio card.
// return 0 if ok, -1 if failed.
int BAE_ReleaseAudioCard(void *threadContext) {
    global_baeout_stream.reset();
    return 0;
}

// Mute/unmute audio. Shutdown amps, etc.
// return 0 if ok, -1 if failed
int BAE_Mute(void) {
    if (global_originalvolume < 0.0f) {
        global_originalvolume = global_baeout_stream->get_volume();
        global_baeout_stream->set_volume(0.0f);
    }
    return 0;
}

int BAE_Unmute(void) {
    if (global_originalvolume >= 0.0f) {
        global_baeout_stream->set_volume(global_originalvolume);
        global_originalvolume = -1.0f;
    }
    return 0;
}

// returns 0 if not muted, 1 if muted
int BAE_IsMuted(void) {
    return (global_originalvolume < 0.0f);
}

void BAE_ProcessRouteBus(int currentRoute, long *pChannels, int count) {

}

void BAE_Idle(void *userContext) {

}

void BAE_UnlockAudioFrameThread(void) {

}

// lock
void BAE_LockAudioFrameThread(void) {

}

// block
void BAE_BlockAudioFrameThread(void) {

}

unsigned long BAE_GetDeviceSamplesPlayedPosition(void) {
    std::uint64_t pos = 0;
    global_baeout_stream->current_frame_position(&pos);

    return static_cast<unsigned long>(pos * global_baeout_stream->get_channels());
}

long BAE_MaxDevices(void) {
    return 1;
}

// set the current device. device is from 0 to BAE_MaxDevices()
// NOTE:    This function needs to function before any other calls may have happened.
//          Also you will need to call BAE_ReleaseAudioCard then BAE_AquireAudioCard
//          in order for the change to take place. deviceParameter is a device specific
//          pointer. Pass NULL if you don't know what to use.
void BAE_SetDeviceID(long deviceID, void *deviceParameter) {

}

// return current device ID, and fills in the deviceParameter with a device specific
// pointer. It will pass NULL if there is nothing to use.
// NOTE: This function needs to function before any other calls may have happened.
long BAE_GetDeviceID(void *deviceParameter) {
    return 0;
}

// get deviceID name 
// NOTE:    This function needs to function before any other calls may have happened.
//          Format of string is a zero terminated comma delinated C string.
//          "platform,method,misc"
//  example "MacOS,Sound Manager 3.0,SndPlayDoubleBuffer"
//          "WinOS,DirectSound,multi threaded"
//          "WinOS,waveOut,multi threaded"
//          "WinOS,VxD,low level hardware"
//          "WinOS,plugin,Director"
void BAE_GetDeviceName(long deviceID, char *cName, unsigned long cNameLength) {
    
}

int BAE_GetAudioBufferCount(void) {
    return 1;
}

// Return the number of bytes used for audio buffer for output to card
long BAE_GetAudioByteBufferSize(void) {
    return 4096;
}

int BAE_NewMutex(BAE_Mutex* lock, char *name, char *file, int lineno) {
#if EKA2L1_PLATFORM(POSIX)
    pthread_mutex_t *pMutex = (pthread_mutex_t *) BAE_Allocate(sizeof(pthread_mutex_t));
    pthread_mutexattr_t attrib;
    pthread_mutexattr_init(&attrib);
    pthread_mutexattr_settype(&attrib, PTHREAD_MUTEX_RECURSIVE);
    // Create reentrant (within same thread) mutex.
    pthread_mutex_init(pMutex, &attrib);
    pthread_mutexattr_destroy(&attrib);
    *lock = (BAE_Mutex) pMutex;
    return 1; // ok
#else
    *lock = CreateMutex(NULL, FALSE, NULL);
    return 1;
#endif
}

void BAE_AcquireMutex(BAE_Mutex mutex) {
#if EKA2L1_PLATFORM(POSIX)
    pthread_mutex_t *pMutex = (pthread_mutex_t*) mutex;
    pthread_mutex_lock(pMutex);
#else
    WaitForSingleObject(reinterpret_cast<HANDLE*>(mutex), INFINITE);
#endif
}

void BAE_ReleaseMutex(BAE_Mutex mutex) {
#if EKA2L1_PLATFORM(POSIX)
    pthread_mutex_t *pMutex = (pthread_mutex_t*) mutex;
    pthread_mutex_unlock(pMutex);
#else
    ReleaseMutex(reinterpret_cast<HANDLE*>(mutex));
#endif
}

void BAE_DestroyMutex(BAE_Mutex mutex) {
#if EKA2L1_PLATFORM(POSIX)
    pthread_mutex_t *pMutex = (pthread_mutex_t*) mutex;
    pthread_mutex_destroy(pMutex);
    BAE_Deallocate(pMutex);
#else
    CloseHandle(reinterpret_cast<HANDLE*>(mutex));
#endif
}
}