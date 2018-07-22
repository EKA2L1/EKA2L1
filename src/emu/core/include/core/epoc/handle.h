#pragma once

#include <core/core.h>
#include <cstdint>

namespace eka2l1::epoc {
    /*! \brief Ownership of the handle. */
    enum TOwnerType {
        //! Owned by process
        /*! This means that when process owned the handle died
         * and the handle hasn't been freed, the handle
         * with be freed.
        */
        EOwnerProcess,

        //! Owned by thread
        /*! This means that when thread owned the handle died 
         * and the handle hasn't been freed, the handle will
         * be freed.
         */
        EOwnerThread
    };

    /*! \brief Symbian class wrapping around a handle. */
    struct RHandleBase {
        //! The actual handle.
        /*! If this handle is not closeable, it will be xor with
         * 0x8000. So we can know if we should let client manually close this 
         * handle or not.
        */
        int iHandle;

        RHandleBase(int aHandle)
            : iHandle(aHandle) {}

        /* \brief Get the kernel object.
         * \returns The kernel object.
        */
        eka2l1::kernel_obj_ptr GetKObject(eka2l1::system *sys);
    };

    /*! \brief Represents a Symbian process handle. 
     *
     * Note that in EKA2L1, process are indicated by its app id,
     * and is not based on kernel_obj.
     */
    struct RProcess {
        //! The app UID
        uint32_t iAppId;

        /* \brief Get the process pointer based on the app uid.
           \returns Pointer to the process
        */
        eka2l1::process_ptr GetProcess(eka2l1::system *sys);
    };

    /*! \brief Find handle struct. 
     *
     * This struct is passed when a SVC call to find a kernel object is invoked.
     * When that operation success, the struct will be filled with kernel
     * object information.
     */
    struct TFindHandle {
        int iHandle;
        int iSpare1;
        int iObjIdLow;
        int iObjIdHigh;
    };
}