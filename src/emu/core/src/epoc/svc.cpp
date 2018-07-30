#include <core/epoc/chunk.h>
#include <core/epoc/dll.h>
#include <core/epoc/hal.h>
#include <core/epoc/handle.h>
#include <core/epoc/svc.h>
#include <core/epoc/tl.h>
#include <core/epoc/uid.h>

#include <common/cvt.h>
#include <common/path.h>
#include <common/random.h>

#ifdef WIN32
#include <Windows.h>
#endif

#include <chrono>
#include <ctime>

namespace eka2l1::epoc {
    /********************************/
    /*    GET/SET EXECUTIVE CALLS   */
    /*                              */
    /* Fast executive call, use for */
    /* get/set local data.          */
    /*                              */
    /********************************/

    /*! \brief Get the current heap allocator */
    BRIDGE_FUNC(eka2l1::ptr<void>, Heap) {
        auto local_data = current_local_data(sys);
        return local_data.heap;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, HeapSwitch, eka2l1::ptr<void> aNewHeap) {
        auto &local_data = current_local_data(sys);
        decltype(aNewHeap) old_heap = local_data.heap;
        local_data.heap = aNewHeap;

        return old_heap;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, TrapHandler) {
        auto local_data = current_local_data(sys);
        return local_data.trap_handler;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, SetTrapHandler, eka2l1::ptr<void> aNewHandler) {
        auto &local_data = current_local_data(sys);
        decltype(aNewHandler) old_handler = local_data.trap_handler;
        local_data.trap_handler = aNewHandler;

        return local_data.trap_handler;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, ActiveScheduler) {
        auto local_data = current_local_data(sys);
        return local_data.scheduler;
    }

    BRIDGE_FUNC(void, SetActiveScheduler, eka2l1::ptr<void> aNewScheduler) {
        auto &local_data = current_local_data(sys);
        local_data.scheduler = aNewScheduler;
    }

    /****************************/
    /* PROCESS */
    /***************************/

    BRIDGE_FUNC(void, ProcessFilename, TInt aProcessHandle, eka2l1::ptr<TDes8> aDes) {
        eka2l1::memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real;

        // 0xffff8000 is a kernel mapping for current process
        if (aProcessHandle != 0xffff8000) {
            // Unlike Symbian, process is not a kernel object here
            // Its handle contains the process's uid
            pr_real = kern->get_process(aProcessHandle);
        } else {
            pr_real = kern->crr_process();
        }

        if (!pr_real) {
            LOG_ERROR("SvcProcessFileName: Invalid process");
            return;
        }

        TDes8 *des = aDes.get(mem);
        des->Assign(sys, pr_real->name());
    }

    BRIDGE_FUNC(void, ProcessType, address pr, eka2l1::ptr<TUidType> uid_type) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr_real;

        // 0xffff8000 is a kernel mapping for current process
        if (pr != 0xffff8000) {
            // Unlike Symbian, process is not a kernel object here
            // Its handle contains the process's uid
            pr_real = kern->get_process(pr);
        } else {
            pr_real = kern->crr_process();
        }

        if (!pr_real) {
            LOG_ERROR("SvcProcessType: Invalid process");
            return;
        }

        TUidType *type = uid_type.get(mem);
        auto &tup = pr_real->get_uid_type();

        type->uid1 = std::get<0>(tup);
        type->uid2 = std::get<1>(tup);
        type->uid3 = std::get<2>(tup);
    }

    BRIDGE_FUNC(TInt, ProcessDataParameterLength, TInt aSlot) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr crr_process = kern->crr_process();

        auto slot = crr_process->get_arg_slot(aSlot);

        if (!slot) {
            return KErrNotFound;
        }

        return static_cast<TInt>(slot->data_size);
    }

    BRIDGE_FUNC(TInt, ProcessGetDataParameter, TInt aSlot, eka2l1::ptr<TUint8> aData, TInt aLength) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->crr_process();

        if (aSlot >= 16 || aSlot < 0) {
            LOG_ERROR("Invalid slot (slot: {} >= 16 or < 0)", aSlot);
            return KErrGeneral;
        }

        auto slot = *pr->get_arg_slot(aSlot);

        if (slot.data_size == -1) {
            return KErrNotFound;
        }

        if (aLength < slot.data_size) {
            return KErrArgument;
        }

        TUint8 *data = aData.get(sys->get_memory_system());

        if (aSlot == 1) {
            std::u16string arg = u"\0l" + pr->get_exe_path();

            if (!pr->get_cmd_args().empty()) {
                arg += u" " + pr->get_cmd_args();
            }

            char src = 0x00;
            char src2 = 0x6C;

            memcpy(data + 2, common::ucs2_to_utf8(arg).data(), arg.length());
            memcpy(data, &src, 1);
            memcpy(data + 1, &src2, 1);

            return slot.data_size;
        }

        TUint8 *ptr2 = eka2l1::ptr<TUint8>(slot.data).get(sys->get_memory_system());
        memcpy(data, ptr2, slot.data_size);

        return slot.data_size;
    }

    BRIDGE_FUNC(void, ProcessSetFlags, TInt aHandle, TUint aClearMask, TUint aSetMask) {
        kernel_system *kern = sys->get_kernel_system();

        process_ptr pr = kern->get_process(aHandle);

        uint32_t org_flags = pr->get_flags();
        uint32_t new_flags = ((org_flags & ~aClearMask) | aSetMask);
        new_flags = (new_flags ^ org_flags);

        pr->set_flags(org_flags ^ new_flags);
    }

    BRIDGE_FUNC(TInt, ProcessSetPriority, TInt aHandle, TInt aProcessPriority) {
        kernel_system *kern = sys->get_kernel_system();
        process_ptr pr = kern->get_process(aHandle);

        if (!pr) {
            return KErrBadHandle;
        }

        pr->set_priority(static_cast<eka2l1::kernel::process_priority>(aProcessPriority));

        return KErrNone;
    }

    BRIDGE_FUNC(void, ProcessResume, TInt aHandle) {
        process_ptr pr = sys->get_kernel_system()->get_process(aHandle);

        if (!pr) {
            return;
        }

        pr->run();
    }

    BRIDGE_FUNC(void, ProcessLogon, TInt aHandle, eka2l1::ptr<TInt> aRequestSts, TBool aRendezvous) {
        process_ptr pr = sys->get_kernel_system()->get_process(aHandle);

        if (!pr) {
            return;
        }

        pr->logon(aRequestSts.get(sys->get_kernel_system()->get_memory_system()), aRendezvous);
    }

    BRIDGE_FUNC(TInt, ProcessLogonCancel, TInt aHandle, eka2l1::ptr<TInt> aRequestSts, TBool aRendezvous) {
        process_ptr pr = sys->get_kernel_system()->get_process(aHandle);

        if (!pr) {
            return KErrBadHandle;
        }

        bool logon_success = pr->logon_cancel(aRequestSts.get(sys->get_kernel_system()->get_memory_system()), aRendezvous);

        if (logon_success) {
            return KErrNone;
        }

        return KErrGeneral;
    }

    /********************/
    /* TLS */
    /*******************/

    BRIDGE_FUNC(eka2l1::ptr<void>, DllTls, TInt aHandle, TInt aDllUid) {
        eka2l1::kernel::thread_local_data dat = current_local_data(sys);

        for (const auto &tls : dat.tls_slots) {
            if (tls.handle == aHandle) {
                return tls.ptr;
            }
        }

        return eka2l1::ptr<void>(0);
    }

    BRIDGE_FUNC(TInt, DllSetTls, TInt aHandle, TInt aDllUid, eka2l1::ptr<void> aPtr) {
        eka2l1::kernel::tls_slot *slot = get_tls_slot(sys, aHandle);

        if (!slot) {
            return KErrNoMemory;
        }

        slot->ptr = aPtr;

        return KErrNone;
    }

    BRIDGE_FUNC(void, DllFreeTLS, TInt iHandle) {
        thread_ptr thr = sys->get_kernel_system()->crr_thread();
        thr->close_tls_slot(*thr->get_tls_slot(iHandle, iHandle));
    }

    /***********************************/
    /* LOCALE */
    /**********************************/

    BRIDGE_FUNC(TInt, UTCOffset) {
        // Stubbed
        return -14400;
    }

    /********************************************/
    /* IPC */
    /*******************************************/

    BRIDGE_FUNC(TInt, ServerCreate, eka2l1::ptr<TDesC8> aServerName, TInt aMode) {
        kernel_system *kern = sys->get_kernel_system();
        std::string server_name = aServerName.get(sys->get_memory_system())->StdString(sys);

        uint32_t handle = kern->create_server(server_name);

        if (handle != INVALID_HANDLE) {
            LOG_TRACE("Server {} created", server_name);
        }

        return handle;
    }

    BRIDGE_FUNC(TInt, ServerReceive, TInt aHandle, eka2l1::ptr<int> aRequestStatus, eka2l1::ptr<TAny> aDataPtr) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        server_ptr server = kern->get_server(aHandle);

        if (!server) {
            return KErrBadHandle;
        }

        server->receive_async_lle(aRequestStatus.get(mem),
            reinterpret_cast<service::message2 *>(aDataPtr.get(mem)));

        return KErrNone;
    }

    BRIDGE_FUNC(TInt, SessionCreate, eka2l1::ptr<TDesC8> aServerName, TInt aMsgSlot, eka2l1::ptr<void> aSec, TInt aMode) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        std::string server_name = aServerName.get(mem)->StdString(sys);
        server_ptr server = kern->get_server_by_name(server_name);

        if (!server) {
            LOG_TRACE("Create session to unexist server: {}", server_name);
            return KErrNotFound;
        }

        uint32_t handle = kern->create_session(server, aMsgSlot);

        if (handle == INVALID_HANDLE) {
            return KErrGeneral;
        }

        LOG_TRACE("New session connected to {} with id {}", server_name, handle);

        return handle;
    }

    BRIDGE_FUNC(TInt, SessionShare, eka2l1::ptr<TInt> aHandle, TInt aShare) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        TInt *handle = aHandle.get(mem);

        session_ptr ss = kern->get_session(*handle);

        if (!ss) {
            return KErrBadHandle;
        }

        if (aShare == 2) {
            // Explicit attach: other process uses IPC can open this handle, so do threads! :D
            ss->set_access_type(kernel::access_type::global_access);
        } else {
            ss->set_access_type(kernel::access_type::local_access);
        }

        int old_handle = *handle;

        // Weird behavior, suddenly it wants to mirror handle then close the old one
        // Clean its identity :D
        *handle = kern->mirror(kern->crr_thread(), *handle, kernel::owner_type::process);
        kern->close(old_handle);

        return KErrNone;
    }

    BRIDGE_FUNC(TInt, SessionSendSync, TInt aHandle, TInt aOrd, eka2l1::ptr<TAny> aIpcArgs,
        eka2l1::ptr<TInt> aStatus) {
        //LOG_TRACE("Send using handle: {}", (aHandle & 0x8000) ? (aHandle & ~0x8000) : (aHandle));

        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        // Dispatch the header
        ipc_arg arg;
        TInt *arg_header = aIpcArgs.cast<TInt>().get(mem);

        if (aIpcArgs) {
            for (uint8_t i = 0; i < 4; i++) {
                arg.args[i] = *arg_header++;
            }

            arg.flag = *arg_header & (((1 << 12) - 1) | (int)ipc_arg_pin::pin_mask);
        }

        session_ptr ss = kern->get_session(aHandle);

        if (!ss) {
            return KErrBadHandle;
        }

        return ss->send_receive_sync(aOrd, arg, aStatus.get(mem));
    }

    /**********************************/
    /* TRAP/LEAVE */
    /*********************************/

    BRIDGE_FUNC(eka2l1::ptr<void>, LeaveStart) {
        LOG_CRITICAL("Leave started!");

        eka2l1::thread_ptr thr = sys->get_kernel_system()->crr_thread();
        thr->increase_leave_depth();

        return current_local_data(sys).trap_handler;
    }

    BRIDGE_FUNC(void, LeaveEnd) {
        eka2l1::thread_ptr thr = sys->get_kernel_system()->crr_thread();
        thr->decrease_leave_depth();

        if (thr->is_invalid_leave()) {
            LOG_CRITICAL("Invalid leave, leave depth is negative!");
        }

        LOG_TRACE("Leave trapped by trap handler.");
    }

    BRIDGE_FUNC(TInt, DebugMask) {
        return 0;
    }

    BRIDGE_FUNC(TInt, DebugMaskIndex, TInt aIdx) {
        return 0;
    }

    BRIDGE_FUNC(TInt, HalFunction, TInt aCagetory, TInt aFunc, eka2l1::ptr<TInt> a1, eka2l1::ptr<TInt> a2) {
        memory_system *mem = sys->get_memory_system();

        int *arg1 = a1.get(mem);
        int *arg2 = a2.get(mem);

        return do_hal(sys, aCagetory, aFunc, arg1, arg2);
    }

    /**********************************/
    /* CHUNK */
    /*********************************/

    BRIDGE_FUNC(TInt, ChunkCreate, TOwnerType aOwnerType, eka2l1::ptr<TDesC8> aName, eka2l1::ptr<TChunkCreate> aChunkCreate) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        TChunkCreate createInfo = *aChunkCreate.get(mem);
        TDesC8 *name = aName.get(mem);

        auto lol
            = name->StdString(sys);

        kernel::chunk_type type;
        kernel::chunk_access access = kernel::chunk_access::local;
        kernel::chunk_attrib att = decltype(att)::none;

        // Fetch chunk type
        if (createInfo.iAtt & TChunkCreate::EDisconnected) {
            type = kernel::chunk_type::disconnected;
        } else if (createInfo.iAtt & TChunkCreate::EDoubleEnded) {
            type = kernel::chunk_type::double_ended;
        } else {
            type = kernel::chunk_type::normal;
        }

        // Fetch chunk access
        if (!(createInfo.iAtt & TChunkCreate::EGlobal)) {
            access = kernel::chunk_access::local;
        } else {
            access = kernel::chunk_access::global;
        }

        if (access == decltype(access)::global && name->Length() == 0) {
            att = kernel::chunk_attrib::anonymous;
        }

        uint32_t handle = kern->create_chunk(name->StdString(sys), createInfo.iInitialBottom, createInfo.iInitialTop,
            createInfo.iMaxSize, prot::read_write, type, access, att,
            aOwnerType == EOwnerProcess ? kernel::owner_type::process : kernel::owner_type::thread);

        if (handle == INVALID_HANDLE) {
            return KErrNoMemory;
        }

        return handle;
    }

    BRIDGE_FUNC(TInt, ChunkMaxSize, TInt aChunkHandle) {
        kernel_obj_ptr obj = RHandleBase(aChunkHandle).GetKObject(sys);

        if (!obj) {
            return KErrBadHandle;
        }

        chunk_ptr chunk = std::dynamic_pointer_cast<kernel::chunk>(obj);
        return chunk->get_max_size();
    }

    BRIDGE_FUNC(eka2l1::ptr<TUint8>, ChunkBase, TInt aChunkHandle) {
        kernel_obj_ptr obj = RHandleBase(aChunkHandle).GetKObject(sys);

        if (!obj) {
            return 0;
        }

        chunk_ptr chunk = std::dynamic_pointer_cast<kernel::chunk>(obj);
        return chunk->base();
    }

    BRIDGE_FUNC(TInt, ChunkAdjust, TInt aChunkHandle, TInt aType, TInt a1, TInt a2) {
        kernel_obj_ptr obj = RHandleBase(aChunkHandle).GetKObject(sys);

        if (!obj) {
            return KErrBadHandle;
        }

        chunk_ptr chunk = std::dynamic_pointer_cast<kernel::chunk>(obj);

        auto fetch = [aType](chunk_ptr chunk, int a1, int a2) -> bool {
            switch (aType) {
            case 0:
                return chunk->adjust(a1);

            case 1:
                return chunk->adjust_de(a1, a2);

            case 2:
                return chunk->commit(a1, a2);

            case 3:
                return chunk->decommit(a1, a2);

            case 4: // Allocate. Afaik this adds more commit size
                return chunk->allocate(a1);

            case 5:
            case 6:
                return KErrNone;
            }

            return KErrGeneral;
        };

        bool res = fetch(chunk, a1, a2);

        if (!res)
            return KErrGeneral;

        return KErrNone;
    }

    /********************/
    /* SYNC PRIMITIVES  */
    /********************/

    BRIDGE_FUNC(TInt, SemaphoreCreate, eka2l1::ptr<TDesC8> aSemaName, TInt aInitCount, TOwnerType aOwnerType) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        TDesC8 *desname = aSemaName.get(mem);
        kernel::owner_type owner = (aOwnerType == EOwnerProcess) ? kernel::owner_type::process : kernel::owner_type::thread;

        uint32_t sema = kern->create_sema(!desname ? "" : desname->StdString(sys),
            aInitCount, 50, owner, !desname ? kernel::access_type::local_access : kernel::access_type::global_access);

        if (sema == INVALID_HANDLE) {
            return KErrGeneral;
        }

        return sema;
    }

    BRIDGE_FUNC(void, WaitForAnyRequest) {
        sys->get_kernel_system()->crr_thread()->wait_for_any_request();
    }

    /***********************************************/
    /* HANDLE FUNCTIONS   */
    /*                    */
    /* Thread independent */
    /**********************************************/

    BRIDGE_FUNC(TInt, ObjectNext, TObjectType aObjectType, eka2l1::ptr<TDes8> aName, eka2l1::ptr<TFindHandle> aHandleFind) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        TFindHandle *handle = aHandleFind.get(mem);
        std::string name = aName.get(mem)->StdString(sys);

        LOG_TRACE("Finding object name: {}", name);

        std::optional<find_handle> info = kern->find_object(name, handle->iHandle, static_cast<kernel::object_type>(aObjectType));

        if (!info) {
            return KErrNotFound;
        }

        handle->iHandle = info->index;
        handle->iObjIdLow = static_cast<uint32_t>(info->object_id);
        handle->iObjIdHigh = info->object_id >> 32;

        return KErrNone;
    }

    BRIDGE_FUNC(TInt, HandleClose, TInt aHandle) {
        if (aHandle & 0x8000) {
            return false;
        }

        bool res = sys->get_kernel_system()->close(aHandle);

        if (res) {
            return KErrNone;
        }

        return KErrBadHandle;
    }

    BRIDGE_FUNC(TInt, HandleDuplicate, TInt aThreadHandle, TOwnerType aOwnerType, TInt aDupHandle) {
        memory_system *mem = sys->get_memory_system();
        kernel_system *kern = sys->get_kernel_system();

        uint32_t res = kern->mirror(kern->get_thread_by_handle(aThreadHandle), aDupHandle,
            (aOwnerType == EOwnerProcess) ? kernel::owner_type::process : kernel::owner_type::thread);

        return res;
    }

    BRIDGE_FUNC(TInt, HandleOpenObject, TObjectType aObjectType, eka2l1::ptr<epoc::TDesC8> aName, TInt aOwnerType) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        std::string obj_name = aName.get(mem)->StdString(sys);

        auto obj_info = kern->find_object(obj_name, 0, static_cast<eka2l1::kernel::object_type>(aObjectType));

        if (!obj_info) {
            return KErrNotFound;
        }

        uint64_t id = obj_info->object_id;
        kernel_obj_ptr obj = kern->get_kernel_obj_by_id(id);

        uint32_t ret_handle = kern->mirror(obj, static_cast<eka2l1::kernel::owner_type>(aOwnerType));

        if (ret_handle != 0xFFFFFFFF) {
            return ret_handle;
        }

        return KErrGeneral;
    }

    BRIDGE_FUNC(void, HandleName, TInt aHandle, eka2l1::ptr<TDes8> aName) {
        kernel_system *kern = sys->get_kernel_system();
        kernel_obj_ptr obj = kern->get_kernel_obj(aHandle);

        if (!obj) {
            if (aHandle == 0xFFFF8001) {
                obj = kern->crr_thread();
            } else
                return;
        }

        TDes8 *desname = aName.get(sys->get_memory_system());
        desname->Assign(sys, obj->name());
    }

    /******************************/
    /* CODE SEGMENT */
    /*****************************/

    BRIDGE_FUNC(TInt, StaticCallList, eka2l1::ptr<TInt> aTotal, eka2l1::ptr<TUint32> aList) {
        memory_system *mem = sys->get_memory_system();

        TUint32 *list_ptr = aList.get(mem);
        TInt *total = aTotal.get(mem);

        std::vector<uint32_t> list = epoc::query_entries(sys);

        *total = static_cast<TInt>(list.size());
        memcpy(list_ptr, list.data(), sizeof(TUint32) * *total);

        return KErrNone;
    }

    /************************/
    /* USER SERVER */
    /***********************/

    BRIDGE_FUNC(TInt, UserSvrRomHeaderAddress) {
        // EKA1
        if ((int)sys->get_kernel_system()->get_epoc_version() <= (int)epocver::epoc6) {
            return 0x50000000;
        }

        // EKA2
        return 0x80000000;
    }

    /************************/
    /*  THREAD  */
    /************************/

    BRIDGE_FUNC(TInt, ThreadKill, TInt aHandle, TExitType aExitType, TInt aReason, eka2l1::ptr<TDesC8> aReasonDes) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        thread_ptr thr = kern->get_thread_by_handle(aHandle);

        if (!thr) {
            return KErrBadHandle;
        }

        std::string exit_cage = aReasonDes.get(mem)->StdString(sys);

        switch (aExitType) {
        case TExitType::panic:
            LOG_TRACE("Thread paniced with cagetory: {} and exit code: {}", exit_cage, aReason);
            break;

        case TExitType::kill:
            LOG_TRACE("Thread forcefully killed with cagetory: {} and exit code: {}", exit_cage, aReason);
            break;

        case TExitType::terminate:
        case TExitType::pending:
            LOG_TRACE("Thread terminated peacefully with cagetory: {} and exit code: {}", exit_cage, aReason);
            break;

        default:
            return KErrArgument;
        }

        kern->get_thread_scheduler()->stop(thr);
        kern->prepare_reschedule();

        return KErrNone;
    }

    BRIDGE_FUNC(TInt, ThreadRename, TInt aHandle, eka2l1::ptr<TDesC8> aName) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        thread_ptr thr;
        TDesC8 *name = aName.get(mem);

        // Current thread handle
        if (aHandle == 0xFFFF8001) {
            thr = kern->crr_thread();
        } else {
            thr = kern->get_thread_by_handle(aHandle);
        }

        if (!thr) {
            return KErrBadHandle;
        }

        std::string new_name = name->StdString(sys);

        LOG_TRACE("Thread with last name: {} renamed to {}", thr->name(), new_name);

        thr->rename(new_name);

        return KErrNone;
    }

    BRIDGE_FUNC(TInt, ThreadProcess, TInt aHandle) {
        kernel_system *kern = sys->get_kernel_system();

        thread_ptr thr = kern->get_thread_by_handle(aHandle);
        return kern->mirror(thr->owning_process(), kernel::owner_type::thread);
    }

    BRIDGE_FUNC(void, ThreadLogon, TInt aHandle, eka2l1::ptr<TInt> aRequestSts, TBool aRendezvous) {
        thread_ptr thr = sys->get_kernel_system()->get_thread_by_handle(aHandle);

        if (!thr) {
            return;
        }

        thr->logon(aRequestSts.get(sys->get_kernel_system()->get_memory_system()), aRendezvous);
    }

    BRIDGE_FUNC(TInt, ThreadLogonCancel, TInt aHandle, eka2l1::ptr<TInt> aRequestSts, TBool aRendezvous) {
        thread_ptr thr = sys->get_kernel_system()->get_thread_by_handle(aHandle);

        if (!thr) {
            return KErrBadHandle;
        }

        bool logon_success = thr->logon_cancel(aRequestSts.get(sys->get_kernel_system()->get_memory_system()), aRendezvous);

        if (logon_success) {
            return KErrNone;
        }

        return KErrGeneral;
    }

    BRIDGE_FUNC(void, ThreadSetFlags, TInt aHandle, TUint aClearMask, TUint aSetMask) {
        kernel_system *kern = sys->get_kernel_system();

        thread_ptr thr = kern->get_thread_by_handle(aHandle);

        uint32_t org_flags = thr->get_flags();
        uint32_t new_flags = ((org_flags & ~aClearMask) | aSetMask);
        new_flags = (new_flags ^ org_flags);

        thr->set_flags(org_flags ^ new_flags);
    }

    /*****************************/
    /* PROPERTY */
    /****************************/

    BRIDGE_FUNC(TInt, PropertyFindGetInt, TInt aCage, TInt aKey, eka2l1::ptr<TInt> aValue) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        property_ptr prop = kern->get_prop(aCage, aKey);

        if (!prop) {
            LOG_WARN("Property not found: cagetory = 0x{:x}, key = 0x{:x}", aCage, aKey);
            return KErrNotFound;
        }

        TInt *val_ptr = aValue.get(mem);
        *val_ptr = prop->get_int();

        return KErrNone;
    }

    BRIDGE_FUNC(TInt, PropertyFindGetBin, TInt aCage, TInt aKey, eka2l1::ptr<TUint8> aData, TInt aDataLength) {
        kernel_system *kern = sys->get_kernel_system();
        memory_system *mem = sys->get_memory_system();

        property_ptr prop = kern->get_prop(aCage, aKey);

        if (!prop) {
            LOG_WARN("Property not found: cagetory = 0x{:x}, key = 0x{:x}", aCage, aKey);
            return KErrNotFound;
        }

        TUint8 *data_ptr = aData.get(mem);
        auto data_vec = prop->get_bin();

        if (data_vec.size() > aDataLength) {
            return KErrNoMemory;
        }

        memcpy(data_ptr, data_vec.data(), data_vec.size());

        return KErrNone;
    }

    BRIDGE_FUNC(TInt, PropertyAttach, TInt aCagetory, TInt aValue, TOwnerType aOwnerType) {
        kernel_system *kern = sys->get_kernel_system();
        property_ptr prop = kern->get_prop(aCagetory, aValue);

        if (!prop) {
            return 0;
        }

        return kern->mirror(prop, static_cast<kernel::owner_type>(aOwnerType));
    }

    /**********************/
    /* TIMER */
    /*********************/
    BRIDGE_FUNC(TInt, TimerCreate) {
        return sys->get_kernel_system()->create_timer("timer" + common::to_string(eka2l1::random()),
            kernel::reset_type::oneshot, kernel::owner_type::process);
    }

    /**********************/
    /* CHANGE NOTIFIER */
    /**********************/
    BRIDGE_FUNC(TInt, ChangeNotifierCreate, TOwnerType aOwner) {
        return sys->get_kernel_system()->create_change_notifier(static_cast<kernel::owner_type>(aOwner));
    }

    const eka2l1::hle::func_map svc_register_funcs_v94 = {
        /* FAST EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00800000, WaitForAnyRequest),
        BRIDGE_REGISTER(0x00800001, Heap),
        BRIDGE_REGISTER(0x00800002, HeapSwitch),
        BRIDGE_REGISTER(0x00800005, ActiveScheduler),
        BRIDGE_REGISTER(0x00800006, SetActiveScheduler),
        BRIDGE_REGISTER(0x00800008, TrapHandler),
        BRIDGE_REGISTER(0x00800009, SetTrapHandler),
        BRIDGE_REGISTER(0x0080000C, DebugMask),
        BRIDGE_REGISTER(0x0080000D, DebugMaskIndex),
        BRIDGE_REGISTER(0x00800013, UserSvrRomHeaderAddress),
        BRIDGE_REGISTER(0x00800019, UTCOffset),
        /* SLOW EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00, ObjectNext),
        BRIDGE_REGISTER(0x01, ChunkBase),
        BRIDGE_REGISTER(0x03, ChunkMaxSize),
        BRIDGE_REGISTER(0x16, ProcessFilename),
        BRIDGE_REGISTER(0x1C, ProcessSetPriority),
        BRIDGE_REGISTER(0x1E, ProcessSetFlags),
        BRIDGE_REGISTER(0x22, ServerReceive),
        BRIDGE_REGISTER(0x27, SessionShare),
        BRIDGE_REGISTER(0x2F, ThreadSetFlags),
        BRIDGE_REGISTER(0x3C, HandleName),
        BRIDGE_REGISTER(0x4D, SessionSendSync),
        BRIDGE_REGISTER(0x4E, DllTls),
        BRIDGE_REGISTER(0x4F, HalFunction),
        BRIDGE_REGISTER(0x6A, HandleClose),
        BRIDGE_REGISTER(0x64, ProcessType),
        BRIDGE_REGISTER(0x6B, ChunkCreate),
        BRIDGE_REGISTER(0x6C, ChunkAdjust),
        BRIDGE_REGISTER(0x6D, HandleOpenObject),
        BRIDGE_REGISTER(0x6E, HandleDuplicate),
        BRIDGE_REGISTER(0x70, SemaphoreCreate),
        BRIDGE_REGISTER(0x73, ThreadKill),
        BRIDGE_REGISTER(0x76, DllSetTls),
        BRIDGE_REGISTER(0x77, DllFreeTLS),
        BRIDGE_REGISTER(0x78, ThreadRename),
        BRIDGE_REGISTER(0x7A, ProcessResume),
        BRIDGE_REGISTER(0x7B, ProcessLogon),
        BRIDGE_REGISTER(0x7C, ProcessLogonCancel),
        BRIDGE_REGISTER(0x7D, ThreadProcess),
        BRIDGE_REGISTER(0x7E, ServerCreate),
        BRIDGE_REGISTER(0x7F, SessionCreate),
        BRIDGE_REGISTER(0x84, TimerCreate),
        BRIDGE_REGISTER(0x87, ChangeNotifierCreate),
        BRIDGE_REGISTER(0xA0, StaticCallList),
        BRIDGE_REGISTER(0xBE, PropertyAttach),
        BRIDGE_REGISTER(0xC5, PropertyFindGetInt),
        BRIDGE_REGISTER(0xC6, PropertyFindGetBin),
        BRIDGE_REGISTER(0xD1, ProcessGetDataParameter),
        BRIDGE_REGISTER(0xD2, ProcessDataParameterLength),
        BRIDGE_REGISTER(0xDF, LeaveStart),
        BRIDGE_REGISTER(0xE0, LeaveEnd)
    };

    const eka2l1::hle::func_map svc_register_funcs_v93 = {
        /* FAST EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00800000, WaitForAnyRequest),
        BRIDGE_REGISTER(0x00800001, Heap),
        BRIDGE_REGISTER(0x00800002, HeapSwitch),
        BRIDGE_REGISTER(0x00800005, ActiveScheduler),
        BRIDGE_REGISTER(0x00800006, SetActiveScheduler),
        BRIDGE_REGISTER(0x00800008, TrapHandler),
        BRIDGE_REGISTER(0x00800009, SetTrapHandler),
        BRIDGE_REGISTER(0x0080000D, DebugMask),
        /* SLOW EXECUTIVE CALL */
        BRIDGE_REGISTER(0x00, ObjectNext)
    };
}