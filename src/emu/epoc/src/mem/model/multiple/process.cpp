#include <epoc/mem/model/multiple/process.h>
#include <epoc/mem/mmu.h>

namespace eka2l1::mem {
    multiple_mem_model_process::multiple_mem_model_process(mmu_base *mmu) 
        : mem_model_process(mmu)
        , addr_space_id_(mmu->rollover_fresh_addr_space())
        , user_local_sec_(local_data, shared_data, mmu->page_size()) {
    }
}
