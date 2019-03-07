#pragma once

#include <cstdint>

namespace eka2l1::epoc {
    using uid = uint32_t;
    
    /*! \brief Triple unique ID. */
    struct uid_type {
        //! The first UID.
        /*! This usually indicates of EXE/DLL 
            for E32Image.
        */
        uint32_t uid1;

        //! The second UID,
        uint32_t uid2;

        //! The third UID.
        /*! This contains unique ID for process. */
        uint32_t uid3;
    };
}
