/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

// Header for resolving SVC call for function which we don't want to implement.
// This exists because of many Symbian devs make their own allocator and switch it
// Some functions are overriden with HLE functions instead of just implementing the svc call
// Because of its complex structure (NMutex, NThread) change over different Symbian update
// But, the public structure still the same for them (RThread, RFastLock), contains only
// a handle. This give us advantage.

#pragma once

#include <epoc/utils/chunk.h>
#include <epoc/utils/des.h>
#include <epoc/utils/dll.h>
#include <epoc/utils/handle.h>
#include <epoc/utils/reqsts.h>
#include <epoc/utils/tl.h>
#include <epoc/utils/sec.h>
#include <epoc/utils/uid.h>

#include <hle/bridge.h>

#include <epoc/hal.h>
#include <epoc/kernel/libmanager.h>

#include <epoc/utils/err.h>
#include <epoc/utils/handle.h>

#include <cstdint>
#include <unordered_map>

namespace eka2l1::kernel {
    struct memory_info;
}

namespace eka2l1::epoc {
    /*! All of the kernel object type. */
    enum TObjectType {
        EThread = 0,
        EProcess,
        EChunk,
        ELibrary,
        ESemaphore,
        EMutex,
        ETimer,
        EServer,
        ESession,
        ELogicalDevice,
        EPhysicalDevice,
        ELogicalChannel,
        EChangeNotifier,
        EUndertaker,
        EMsgQueue,
        EPropertyRef,
        ECondVar,
        EShPool,
        EShBuf,
        ENumObjectTypes, // number of DObject-derived types
        EObjectTypeAny = -1
    };

    enum class TExitType {
        kill,
        terminate,
        panic,
        pending
    };

    enum property_type {
        property_type_int,
        property_type_byte_array,
        property_type_text = property_type_byte_array,
        property_type_large_byte_array,
        property_type_large_text = property_type_large_byte_array,
        property_type_limit,
        property_type_mask = 0xff
    };

    struct TPropertyInfo {
        std::uint32_t attrib;
        std::uint16_t size;
        property_type type;
        epoc::security_policy read_policy;
        epoc::security_policy write_policy;
    };

    /*! \brief Get the thread-local allocator. 
     * \returns The pointer to the allocator. 
     */
    BRIDGE_FUNC(eka2l1::ptr<void>, Heap);

    /*! \brief Switch the old thread-local allocator with a new one. 
     * \param aNewHeap Pointer to the new heap in virtual memory.
     * \returns The pointer to the old heap allocator.
     */
    BRIDGE_FUNC(eka2l1::ptr<void>, HeapSwitch, eka2l1::ptr<void> aNewHeap);

    /*! \brief Set the thread-local trap allocator. 
     * \returns The trap handler.
     */
    BRIDGE_FUNC(eka2l1::ptr<void>, TrapHandler);

    /*! \brief Set the thread-local trap handler. 
     * \param aNewHandler Pointer to the new trap handler.
     * \returns The old trap handler.
    */
    BRIDGE_FUNC(eka2l1::ptr<void>, SetTrapHandler, eka2l1::ptr<void> aNewHandler);

    /*! \brief Get the thread-local active scheduler. 
     * \returns The active scheduler
    */
    BRIDGE_FUNC(eka2l1::ptr<void>, ActiveScheduler);

    /*! \brief Set the thread-local active scheduler. 
     * \param The pointer to new active scheduler. 
     */
    BRIDGE_FUNC(void, SetActiveScheduler, eka2l1::ptr<void> aNewScheduler);

    /*! \brief Get the process filename (e.g apprun.exe). 
     *
     * This returns the filename from the running path (C:\sys\bin or C:\system\apps). 
     *
     * \param aProcessHandle The process handle. 0xFFFF8000 is the current process.
     * \param aDes Pointer to the name descriptor.
     */
    BRIDGE_FUNC(void, ProcessFilename, std::int32_t aProcessHandle, eka2l1::ptr<eka2l1::epoc::des8> aDes);

    /*! \brief Get the process triple uid. 
     *
     * Each Symbian process has triple uid. The first uid indicates if the E32Image is exe or dll/
     * The third is for the process's own UID.
     *
     * \param pr The process handle. 0xFFFF8000 is the current process.
     * \param uid_type Pointer to the uid type (triple uid).
     */
    BRIDGE_FUNC(void, ProcessType, address pr, eka2l1::ptr<epoc::uid_type> uid_type);

    /*! \brief Get the size of the data in the process parameter slot. 
     * 
     * \param aSlot The slot index. Range from 0-15.
     * \returns The slot data size. Error code if there is problem.
    */
    BRIDGE_FUNC(std::int32_t, ProcessDataParameterLength, std::int32_t aSlot);

    /*! \brief Get the environment data parameter. 
     *
     * \param aSlot The slot index to get data from.
     * \param aData Pointer to the destination.
     * \param aLength Max size the destination can hold.
     *
     * \returns The slot data size, else error code.
     */
    BRIDGE_FUNC(std::int32_t, ProcessGetDataParameter, std::int32_t aSlot, eka2l1::ptr<std::uint8_t> aData, std::int32_t aLength);

    /**
     * \brief Set a process's environment data parameter.
     * 
     * This requires the caller of this system call to be the parent of given process.
     * 
     * \param aHandle Handle of the target process.
     * \param aSlot   The slot index to get data from.
     * \param aData   Pointers to data to be set.
     * \param aSize   Size of data to be set.
     * 
     * \returns KErrNone if success, KErrPermissionDenined if the caller is not the parent of given
     *          process, else other system related error.
     */
    BRIDGE_FUNC(std::int32_t, ProcessSetDataParameter, std::int32_t aHandle, std::int32_t aSlot, eka2l1::ptr<std::uint8_t> aData, std::int32_t aDataSize);

    /*! \brief Set the process flags. 
     * \param aHandle The process handle.
     * \param aClearMask The flags to clear mask.
     * \param aSetMask The flags to set mask.
     */
    BRIDGE_FUNC(void, ProcessSetFlags, std::int32_t aHandle, std::uint32_t aClearMask, std::uint32_t aSetMask);

    /*! \brief Get the thread local storage of a DLL. 
     *
     * \param aHandle Virtual pointer to the first entry of the dll
     * \param aDllUid The third UID of the DLL. This is usually zero, so
     * the kernel will figure out the uid itself.
     *
     * \returns Thread local storage.
    */
    BRIDGE_FUNC(eka2l1::ptr<void>, DllTls, std::int32_t aHandle, std::int32_t aDllUid);

    /*! \brief Set the thread local storage of a DLL. 
     *
     * \param aHandle Virtual pointer to the first entry of the dll
     * \param aDllUid The third UID of the DLL. This is usually zero, so
     * the kernel will figure out the uid itself.
     * \param aPtr The data to be set.
     *
     * \returns 0 if success, else error code.
    */
    BRIDGE_FUNC(std::int32_t, DllSetTls, std::int32_t aHandle, std::int32_t aDllUid, eka2l1::ptr<void> aPtr);

    /*! \brief Free the thread local storage of a DLL. 
     *
     * \param aHandle Virtual pointer to the first entry of the dll
     *
     * \returns 0 if success, else error code.
    */
    BRIDGE_FUNC(void, DllFreeTLS, std::int32_t iHandle);

    /**
     * \brief Get the UTC offset in seconds. 
     * \returns The UTC offset, in seconds.
     */
    BRIDGE_FUNC(std::int32_t, UTCOffset);

    /*! \brief Create a new session. 
     *
     * \param aServerName Pointer to the server name.
     * \param aMsgSlot Total async messages slot. Use -1 for using sync slot of the current thread.
     * \param aSec Security dependency. Ignored in here.
     * \param aMode Session mode. Ignored here.
     * \returns The session handle.
    */
    BRIDGE_FUNC(std::int32_t, SessionCreate, eka2l1::ptr<eka2l1::epoc::desc8> aServerName, std::int32_t aMsgSlot, eka2l1::ptr<void> aSec, std::int32_t aMode);

    /*! \brief Share the session to local process or all process. 
     *
     * \param aHandle Pointer to the real handle. Questionable.
     * \param aShare Share mode. 2 for globally and 1 for locally.
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(std::int32_t, SessionShare, eka2l1::ptr<std::int32_t> aHandle, std::int32_t aShare);

    /*! \brief Send the IPC function and arguments using local thread sync message. 
     *
     * \param aHandle The session handle.
     * \param aOrd The function opcode.
     * \param aIpcArgs Pointer to the ipc arguments;
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(std::int32_t, SessionSendSync, std::int32_t aHandle, std::int32_t aOrd, eka2l1::ptr<void> aIpcArgs,
        eka2l1::ptr<epoc::request_status> aStatus);

    /*! \brief Start the leave. 
     *
     * \returns Pointer to the thread-local trap handler.
     */
    BRIDGE_FUNC(eka2l1::ptr<void>, LeaveStart);

    /*! \brief Get the debug mask. */
    BRIDGE_FUNC(std::int32_t, DebugMask);

    /*! \brief Execute an HAL function. 
     *
     * \param aCagetory The HAL cagetory.
     * \param aFunc The HAL function opcode in cagetory.
     * \param a1 Pointer to the first argument to be passed to HAL.
     * \param a2 Pointer to the second argument to be passed to HAL
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(std::int32_t, HalFunction, std::int32_t aCagetory, std::int32_t aFunc, eka2l1::ptr<std::int32_t> a1, eka2l1::ptr<std::int32_t> a2);

    /*! \brief Create a new chunk.
     *
     * \param aOwnerType Owner of the handle.
     * \param aName Chunk name. Ignored if the chunk is local and no force naming.
     * \param aChunkCreate Chunk creation info.
     *
     * \returns Error code if problems. Else return the handle.
    */
    BRIDGE_FUNC(std::int32_t, ChunkCreate, epoc::owner_type aOwnerType, eka2l1::ptr<eka2l1::epoc::desc8> aName, eka2l1::ptr<epoc::chunk_create> aChunkCreate);

    /*! \brief Get the max size of the chunk. 
     * 
     * \param aChunkHandle The handle of the chunk.
     * \returns The chunk size or error code.
     */
    BRIDGE_FUNC(std::int32_t, ChunkMaxSize, std::int32_t aChunkHandle);

    /*!\ brief Get the chunk base. 
     * 
     * \param aChunkHandle The handle of the chunk.
     * \returns The chunk base pointer.
    */
    BRIDGE_FUNC(eka2l1::ptr<std::uint8_t>, ChunkBase, std::int32_t aChunkHandle);

    /*! \brief Adjust the chunk based on the type. 
     *
     * Unlike the function described in API, the SVC call is executed base on the type:
     * auto adjust, adjust double end (top and bottom), commit, decommit, allocate, etc...
     *
     * \param aChunkHandle The handle of the chunk.
     * \param aType Adjust type.
     * \param a1 First argument.
     * \param a2 Second argument.
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(std::int32_t, ChunkAdjust, std::int32_t aChunkHandle, std::int32_t aType, std::int32_t a1, std::int32_t a2);

    /*! \brief Create new semaphore. 
     * 
     * \param aSemaName The semaphore name.
     * \param aInitCount Semaphore initial count.
     * \param aOwnerType Ownership of this handle.
     * \returns Error code or handle.
     */
    BRIDGE_FUNC(std::int32_t, SemaphoreCreate, eka2l1::ptr<eka2l1::epoc::desc8> aSemaName, std::int32_t aInitCount, epoc::owner_type aOwnerType);

    /*! \brief Wait for any request to finish. 
     *
     * This will wait for the signal semaphore of the current thread until any
     * request signal that semaphore.
     */
    BRIDGE_FUNC(void, WaitForAnyRequest);

    /*! \brief Find the kernel object with the specified type and name. 
     * 
     * \param aObjectType The kernel object type.
     * \param aName A pointer to a 8-bit descriptor (Symbian EKA1 is 16-bit) contains the name.
     * \param aHandleFind Pointer to the find handle struct. Operation success results in this struct filled.
     *
     * \returns KErrNone if success, else error code.
     */
    BRIDGE_FUNC(std::int32_t, ObjectNext, TObjectType aObjectType, eka2l1::ptr<des8> aName, eka2l1::ptr<epoc::find_handle> aHandleFind);

    /*! \brief Close a handle. If there is no duplicate handle or another reference handle open, 
     *  call Destroy to destroy the kernel object 
     * \param aHandle The handle 
     */
    BRIDGE_FUNC(std::int32_t, HandleClose, std::int32_t aHandle);

    /*! \brief Duplicate the handle. Create another reference to the real kernel object.
      * \param aThreadHandle The thread owns the handle. 0xFFFF8001 for current thread. Ignored.
      * \param aOwnerType New owner type for new handle. 
      * \param aDupHandle Handle to be duplicated.
      * 
      * \returns Error code or new handle.
    */
    BRIDGE_FUNC(std::int32_t, HandleDuplicate, std::int32_t aThreadHandle, epoc::owner_type aOwnerType, std::int32_t aDupHandle);

    /*! \brief Open a handle based on the name and object type.
     *
     * \param aObjectType Object type.
     * \param aName The object name.
     * \param aOwnerType The ownership of the handle to be opened.
     *
     * \returns Error code or handle.
     */
    BRIDGE_FUNC(std::int32_t, HandleOpenObject, TObjectType aObjectType, eka2l1::ptr<eka2l1::epoc::desc8> aName, std::int32_t aOwnerType);

    /*! \brief Get the name of the object handle points to. */
    BRIDGE_FUNC(void, HandleName, std::int32_t aHandle, eka2l1::ptr<eka2l1::epoc::des8> aName);

    /*! \brief Get all the first entry points of DLL the app loaded. 
     *
     * \param aTotal Pointer to the length of the destination list. After done lisiting, this variable
     * will be changed to the number of DLL loaded.
     *
     * \param aList Destination entry points list.
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(std::int32_t, StaticCallList, eka2l1::ptr<std::int32_t> aTotal, eka2l1::ptr<std::uint32_t> aList);

    /*! \brief Get the ROM address. 
     * 
     * For EKA1 this is 0x50000000, and EKA2 is 0x80000000. More information on memory map,
     * see wiki.
     */
    BRIDGE_FUNC(std::int32_t, UserSvrRomHeaderAddress);

    /*! \brief Rename the thread.
     * 
     * \param aHandle The handle to the thread. 0xFFFF8001 for current thread.
     * \param aName New thread name
     *
     * \returns Error code. KErrNone if success.
    */
    BRIDGE_FUNC(std::int32_t, ThreadRename, std::int32_t aHandle, eka2l1::ptr<eka2l1::epoc::desc8> aName);

    /*! \brief Get the integer data of the proeprty with the provided cagetory and key. 
     *
     * \param aCage The property cagetory.
     * \param aKey The property key.
     * \param aValue Pointer to the destination to write data to.
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(std::int32_t, PropertyFindGetInt, std::int32_t aCage, std::int32_t aKey, eka2l1::ptr<std::int32_t> aValue);

    /*! \brief Get the binary data of the proeprty with the provided cagetory and key. 
     *
     * \param aCage The property cagetory.
     * \param aKey The property key.
     * \param aValue Pointer to the destination to write data to.
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(std::int32_t, PropertyFindGetBin, std::int32_t aCage, std::int32_t aKey, eka2l1::ptr<std::uint8_t> aData, std::int32_t aDataLength);

    /**
     * \brief Prints text to debug port or host debugger.
     * 
     * \param aDes Descriptor to prints.
     * \param aMode Undocumented.
     */
    BRIDGE_FUNC(void, DebugPrint, eka2l1::ptr<desc8> aDes, std::int32_t aMode);

    /**
     * \brief Get the current executable's exception descriptor.
     * 
     * Exception descriptor contains informations which will be used by the exception handler
     * to speed up unwinding.
     * 
     * This is not confirms yet to be true.
     * 
     * \return Address of the exception descriptor.
     * \see    StaticCallList
     */
    BRIDGE_FUNC(address, ExceptionDescriptor, address aInAddr);

    /**
     * \brief Increase value by 1 if it's positive (> 0)
     * \returns Original value
    */
    BRIDGE_FUNC(std::int32_t, SafeInc32, eka2l1::ptr<std::int32_t> aVal);

    /**
     * \brief Create a new change notifier object.
     * 
     * Change notifier is a kernel object that will notify users about environment change
     * on the phone. For example, UTC offset change, time change, global data change, and more.
     * 
     * \param aOwner The owner of this object's handle.
     * \returns < 0 means an error, else the handle to this change notifier.
     */
    BRIDGE_FUNC(std::int32_t, ChangeNotifierCreate, epoc::owner_type aOwner);

    /**
     * \brief Logon an existing change notifier.
     * 
     * When users logon an change notifier, they will be notified when there is environment change
     * on the phone.
     * 
     * Logon can be cancel by using ChangeNotifierLogonCancel
     * 
     * \param aHandle        Handle to this change notifier.
     * \param aRequestStatus Pointer to the status to be notified.
     * 
     * \returns KErrNone if success, else other system's related error.
     * \see     ChangeNotifierLogonCancel
     */
    BRIDGE_FUNC(std::int32_t, ChangeNotifierLogon, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestStatus);

    /**
     * \brief Cancel a timer currently running.
     * \param aHandle Handle to the timer. 
     */
    BRIDGE_FUNC(void, TimerCancel, std::int32_t aHandle);

    /**
     * \brief Let a timer running and notify the client when the time is reached at UTC.
     * 
     * This call will let the timer run even when phone is down.
     * 
     * \param aHandle         Handle to the timer.
     * \param aRequestStatus  Status to be notified.
     * \param aMicrosecondsAt Time for timer to run, in microseconds.
     * 
     * \see TimerAfter
     */
    BRIDGE_FUNC(void, TimerAtUtc, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestStatus, std::uint64_t aMicroSecondsAt);

    /**
     * \brief Let a timer running and notify the client when the time is reached.
     * 
     * If the phone is down, and the time is reached, this will cancel the timer if delay is too much.
     * 
     * \param aHandle         Handle to the timer.
     * \param aRequestStatus  Status to be notified.
     * \param aMicrosecondsAt Time for timer to run, in microseconds.
     * 
     * \see TimerAtUtc
     */
    BRIDGE_FUNC(void, TimerAfter, std::int32_t aHandle, eka2l1::ptr<epoc::request_status> aRequestStatus, std::int32_t aMicroSeconds);

    /**
     * \brief Create a timer.
     * 
     * Timer is a countdown object. Users can request the timer to notify the client after a certain amount
     * of time is passed. This will be handful in many situations, like alarm, or wake up the thread while it's
     * idling, to make it do something useful.
     * 
     * The handle of the timer will always be owned by the process.
     * 
     * \returns Handle to the timer if > 0, else error.
     */
    BRIDGE_FUNC(std::int32_t, TimerCreate);

    /**
     * \brief Get exit type of process.
     * 
     * Requires the process to first be killed by ProcessKill.
     * 
     * \param aHandle Handle to the target process.
     * \returns Exit type (> 0), else other system related error codes.
     */
    BRIDGE_FUNC(std::int32_t, ProcessExitType, std::int32_t aHandle);
    
    /**
     * \brief Notify rendezvous requests with a code.
     * 
     * Threads that requests rendezvous on the current process, that is waiting for this
     * request, will be notified with the given code.
     * 
     * \param aRendezvousCode The code to notify all waiting threads.
     * \see   ProcessLogon ProcessLogonCancel
     */
    BRIDGE_FUNC(void, ProcessRendezvous, std::int32_t aRendezvousCode);

    /**
     * \brief Get a process's memory info.
     * 
     * Memory info mentioning here is process's codeseg address, .data, .bss addresses, code size,
     * and eventually more.
     * 
     * \param aHandle Handle to target process.
     * \param aInfo   Pointer to the memory info struct to be filled.
     * 
     * \returns KErrNone if success, else other system's related error.
     */
    BRIDGE_FUNC(std::int32_t, ProcessGetMemoryInfo, std::int32_t aHandle, eka2l1::ptr<kernel::memory_info> aInfo);

    /**
     * \brief Get ID of given process.
     * \param aHandle Handle to target process.
     */
    BRIDGE_FUNC(std::int32_t, ProcessGetId, std::int32_t aHandle);
    
    /**
     * \brief   Get length of command line string passed to a process.
     * 
     * \param   aHandle Handle to target process.
     * \returns The length of the command line string.
     */
    BRIDGE_FUNC(std::int32_t, ProcessCommandLineLength, std::int32_t aHandle);

    /**
     * \brief Get the command line string.
     * 
     * \param aHandle Handle to target process.
     * \param aData   Descriptor which will hold the cmd line string data.
     */
    BRIDGE_FUNC(void, ProcessCommandLine, std::int32_t aHandle, eka2l1::ptr<epoc::des8> aData);

    /**
     * \brief Set mask for given process.
     * 
     * \param aHandle    Handle to target process.
     * \param aClearMask The mask to be cleared for given process flags.
     * \param aSetMask   The mask to be set for given process flags.
     */
    BRIDGE_FUNC(void, ProcessSetFlags, std::int32_t aHandle, std::uint32_t aClearMask, std::uint32_t aSetMask);

    //! The SVC map for Symbian S60v3.
    extern const eka2l1::hle::func_map svc_register_funcs_v93;

    //! The SVC map for Symbian S60v5
    extern const eka2l1::hle::func_map svc_register_funcs_v94;
}
