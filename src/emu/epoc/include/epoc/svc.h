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
#include <epoc/utils/uid.h>

#include <hle/bridge.h>

#include <epoc/hal.h>
#include <epoc/kernel/libmanager.h>

#include <common/e32inc.h>
#include <e32err.h>

#include <unordered_map>

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

    enum TPropertyType {
        EInt,
        EByteArray,
        EText = EByteArray,
        ELargeByteArray,
        ELargeText = ELargeByteArray,
        ETypeLimit,
        ETypeMask = 0xff
    };

    struct TSecurityPolicy {
        TUint8 iType;
        TUint8 iCaps[3];

        union {
            TUint32 iSecureId;
            TUint32 iVendorId;
            TUint8 iExtraCaps[4];
        };
    };

    struct TPropertyInfo {
        TUint iAttrib;
        TUint16 iSize;
        TPropertyType iType;
        TSecurityPolicy iReadPolicy;
        TSecurityPolicy iWritePolicy;
    };

    struct TSecurityInfo {
        TUint iSecureId;
        TUint iVendorId;
        TUint iCaps[2];
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
    BRIDGE_FUNC(void, ProcessFilename, TInt aProcessHandle, eka2l1::ptr<eka2l1::epoc::des8> aDes);

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
    BRIDGE_FUNC(TInt, ProcessDataParameterLength, TInt aSlot);

    /*! \brief Get the environment data parameter. 
     *
     * \param aSlot The slot index to get data from..
     * \param Pointer to the destination.
     * \param Max size the destination can hold.
     *
     * \returns The slot data size, else error code.
     */
    BRIDGE_FUNC(TInt, ProcessGetDataParameter, TInt aSlot, eka2l1::ptr<TUint8> aData, TInt aLength);

    /*! \brief Set the process flags. 
     * \param aHandle The process handle.
     * \param aClearMask The flags to clear mask.
     * \param aSetMask The flags to set mask.
     */
    BRIDGE_FUNC(void, ProcessSetFlags, TInt aHandle, TUint aClearMask, TUint aSetMask);

    /*! \brief Get the thread local storage of a DLL. 
     *
     * \param aHandle Virtual pointer to the first entry of the dll
     * \param aDllUid The third UID of the DLL. This is usually zero, so
     * the kernel will figure out the uid itself.
     *
     * \returns Thread local storage.
    */
    BRIDGE_FUNC(eka2l1::ptr<void>, DllTls, TInt aHandle, TInt aDllUid);

    /*! \brief Set the thread local storage of a DLL. 
     *
     * \param aHandle Virtual pointer to the first entry of the dll
     * \param aDllUid The third UID of the DLL. This is usually zero, so
     * the kernel will figure out the uid itself.
     * \param aPtr The data to be set.
     *
     * \returns 0 if success, else error code.
    */
    BRIDGE_FUNC(TInt, DllSetTls, TInt aHandle, TInt aDllUid, eka2l1::ptr<void> aPtr);

    /*! \brief Free the thread local storage of a DLL. 
     *
     * \param aHandle Virtual pointer to the first entry of the dll
     *
     * \returns 0 if success, else error code.
    */
    BRIDGE_FUNC(void, DllFreeTLS, TInt iHandle);

    /*! \brief Get the UTC offset. */
    BRIDGE_FUNC(TInt, UTCOffset);

    /*! \brief Create a new session. 
     *
     * \param aServerName Pointer to the server name.
     * \param aMsgSlot Total async messages slot. Use -1 for using sync slot of the current thread.
     * \param aSec Security dependency. Ignored in here.
     * \param aMode Session mode. Ignored here.
     * \returns The session handle.
    */
    BRIDGE_FUNC(TInt, SessionCreate, eka2l1::ptr<eka2l1::epoc::desc8> aServerName, TInt aMsgSlot, eka2l1::ptr<void> aSec, TInt aMode);

    /*! \brief Share the session to local process or all process. 
     *
     * \param aHandle Pointer to the real handle. Questionable.
     * \param aShare Share mode. 2 for globally and 1 for locally.
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(TInt, SessionShare, eka2l1::ptr<TInt> aHandle, TInt aShare);

    /*! \brief Send the IPC function and arguments using local thread sync message. 
     *
     * \param aHandle The session handle.
     * \param aOrd The function opcode.
     * \param aIpcArgs Pointer to the ipc arguments;
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(TInt, SessionSendSync, TInt aHandle, TInt aOrd, eka2l1::ptr<TAny> aIpcArgs,
        eka2l1::ptr<epoc::request_status> aStatus);

    /*! \brief Start the leave. 
     *
     * \returns Pointer to the thread-local trap handler.
     */
    BRIDGE_FUNC(eka2l1::ptr<void>, LeaveStart);

    /*! \brief Get the debug mask. */
    BRIDGE_FUNC(TInt, DebugMask);

    /*! \brief Execute an HAL function. 
     *
     * \param aCagetory The HAL cagetory.
     * \param aFunc The HAL function opcode in cagetory.
     * \param a1 Pointer to the first argument to be passed to HAL.
     * \param a2 Pointer to the second argument to be passed to HAL
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(TInt, HalFunction, TInt aCagetory, TInt aFunc, eka2l1::ptr<TInt> a1, eka2l1::ptr<TInt> a2);

    /*! \brief Create a new chunk.
     *
     * \param aOwnerType Owner of the handle.
     * \param aName Chunk name. Ignored if the chunk is local and no force naming.
     * \param aChunkCreate Chunk creation info.
     *
     * \returns Error code if problems. Else return the handle.
    */
    BRIDGE_FUNC(TInt, ChunkCreate, TOwnerType aOwnerType, eka2l1::ptr<eka2l1::epoc::desc8> aName, eka2l1::ptr<TChunkCreate> aChunkCreate);

    /*! \brief Get the max size of the chunk. 
     * 
     * \param aChunkHandle The handle of the chunk.
     * \returns The chunk size or error code.
     */
    BRIDGE_FUNC(TInt, ChunkMaxSize, TInt aChunkHandle);

    /*!\ brief Get the chunk base. 
     * 
     * \param aChunkHandle The handle of the chunk.
     * \returns The chunk base pointer.
    */
    BRIDGE_FUNC(eka2l1::ptr<TUint8>, ChunkBase, TInt aChunkHandle);

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
    BRIDGE_FUNC(TInt, ChunkAdjust, TInt aChunkHandle, TInt aType, TInt a1, TInt a2);

    /*! \brief Create new semaphore. 
     * 
     * \param aSemaName The semaphore name.
     * \param aInitCount Semaphore initial count.
     * \param aOwnerType Ownership of this handle.
     * \returns Error code or handle.
     */
    BRIDGE_FUNC(TInt, SemaphoreCreate, eka2l1::ptr<eka2l1::epoc::desc8> aSemaName, TInt aInitCount, TOwnerType aOwnerType);

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
    BRIDGE_FUNC(TInt, ObjectNext, TObjectType aObjectType, eka2l1::ptr<des8> aName, eka2l1::ptr<TFindHandle> aHandleFind);

    /*! \brief Close a handle. If there is no duplicate handle or another reference handle open, 
     *  call Destroy to destroy the kernel object 
     * \param aHandle The handle 
     */
    BRIDGE_FUNC(TInt, HandleClose, TInt aHandle);

    /*! \brief Duplicate the handle. Create another reference to the real kernel object.
      * \param aThreadHandle The thread owns the handle. 0xFFFF8001 for current thread. Ignored.
      * \param aOwnerType New owner type for new handle. 
      * \param aDupHandle Handle to be duplicated.
      * 
      * \returns Error code or new handle.
    */
    BRIDGE_FUNC(TInt, HandleDuplicate, TInt aThreadHandle, TOwnerType aOwnerType, TInt aDupHandle);

    /*! \brief Open a handle based on the name and object type.
     *
     * \param aObjectType Object type.
     * \param aName The object name.
     * \param aOwnerType The ownership of the handle to be opened.
     *
     * \returns Error code or handle.
     */
    BRIDGE_FUNC(TInt, HandleOpenObject, TObjectType aObjectType, eka2l1::ptr<eka2l1::epoc::desc8> aName, TInt aOwnerType);

    /*! \brief Get the name of the object handle points to. */
    BRIDGE_FUNC(void, HandleName, TInt aHandle, eka2l1::ptr<eka2l1::epoc::des8> aName);

    /*! \brief Get all the first entry points of DLL the app loaded. 
     *
     * \param aTotal Pointer to the length of the destination list. After done lisiting, this variable
     * will be changed to the number of DLL loaded.
     *
     * \param aList Destination entry points list.
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(TInt, StaticCallList, eka2l1::ptr<TInt> aTotal, eka2l1::ptr<TUint32> aList);

    /*! \brief Get the ROM address. 
     * 
     * For EKA1 this is 0x50000000, and EKA2 is 0x80000000. More information on memory map,
     * see wiki.
     */
    BRIDGE_FUNC(TInt, UserSvrRomHeaderAddress);

    /*! \brief Rename the thread.
     * 
     * \param aHandle The handle to the thread. 0xFFFF8001 for current thread.
     * \param aName New thread name
     *
     * \returns Error code. KErrNone if success.
    */
    BRIDGE_FUNC(TInt, ThreadRename, TInt aHandle, eka2l1::ptr<eka2l1::epoc::desc8> aName);

    /*! \brief Get the integer data of the proeprty with the provided cagetory and key. 
     *
     * \param aCage The property cagetory.
     * \param aKey The property key.
     * \param aValue Pointer to the destination to write data to.
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(TInt, PropertyFindGetInt, TInt aCage, TInt aKey, eka2l1::ptr<TInt> aValue);

    /*! \brief Get the binary data of the proeprty with the provided cagetory and key. 
     *
     * \param aCage The property cagetory.
     * \param aKey The property key.
     * \param aValue Pointer to the destination to write data to.
     *
     * \returns Error code. KErrNone if success.
     */
    BRIDGE_FUNC(TInt, PropertyFindGetBin, TInt aCage, TInt aKey, eka2l1::ptr<TUint8> aData, TInt aDataLength);

    //! The SVC map for Symbian S60v3.
    extern const eka2l1::hle::func_map svc_register_funcs_v93;

    //! The SVC map for Symbian S60v5
    extern const eka2l1::hle::func_map svc_register_funcs_v94;
}
