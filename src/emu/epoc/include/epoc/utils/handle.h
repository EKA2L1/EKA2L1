#pragma once

#include <cstdint>
#include <memory>

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
