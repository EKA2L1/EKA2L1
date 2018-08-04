#pragma once

#include <common/e32inc.h>
#include <e32def.h>

namespace eka2l1::epoc {
    /*! \brief Chunk create info.
     *
     * This struct is passed on creation of a chunk.
    */
    struct TChunkCreate {
        //! Attributes for chunk creation that are used by both euser and the kernel
        // by classes TChunkCreateInfo and SChunkCreateInfo, respectively.
        enum TChunkCreateAtt {
            ENormal = 0x00000000,
            EDoubleEnded = 0x00000001,
            EDisconnected = 0x00000002,
            ECache = 0x00000003,
            EMappingMask = 0x0000000f,
            ELocal = 0x00000000,
            EGlobal = 0x00000010,
            EData = 0x00000000,
            ECode = 0x00000020,
            EMemoryNotOwned = 0x00000040,

            // Force local chunk to be named.  Only required for thread heap
            // chunks, all other local chunks should be nameless.
            ELocalNamed = 0x000000080,

            // Make global chunk read only to all processes but the controlling owner
            EReadOnly = 0x000000100,

            // Paging attributes for chunks.
            EPagingUnspec = 0x00000000,
            EPaged = 0x80000000,
            EUnpaged = 0x40000000,
            EPagingMask = EPaged | EUnpaged,

            EChunkCreateAttMask = EMappingMask | EGlobal | ECode | ELocalNamed | EReadOnly | EPagingMask,
        };

    public:
        TUint iAtt;
        TBool iForceFixed;
        TInt iInitialBottom;
        TInt iInitialTop;
        TInt iMaxSize;
        TUint8 iClearByte;
    };
}