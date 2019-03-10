#pragma once

#include <epoc/loader/mbm.h>
#include <epoc/utils/uid.h>
#include <epoc/ptr.h>

namespace eka2l1::epoc {
    struct bitwise_bitmap {
        uid                uid_;
        eka2l1::ptr<void>  allocator_;
        eka2l1::ptr<void>  pile_;
        int                byte_width_;
        loader::sbm_header header_;
        int                spare1_;
        int                data_offset_;
        bool               compressed_in_ram_;
    };

    constexpr epoc::uid bitwise_bitmap_uid = 0x10000040;
}
