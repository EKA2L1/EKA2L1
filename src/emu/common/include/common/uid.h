#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace eka2l1::epoc {
    using uid = uint32_t;

    /*! \brief Triple unique ID. */
    struct uid_type {
        //! The first UID.
        /*! This usually indicates of EXE/DLL 
            for E32Image.
        */
        uid uid1;

        //! The second UID,
        uid uid2;

        //! The third UID.
        /*! This contains unique ID for process. */
        uid uid3;

        uid operator[](const std::size_t idx) {
            switch (idx) {
            case 0: {
                return uid1;
            }

            case 1: {
                return uid2;
            }

            case 2: {
                return uid3;
            }

            default:
                break;
            }

            throw std::runtime_error("UID index out of range!");
            return 0;
        }
    };
}
