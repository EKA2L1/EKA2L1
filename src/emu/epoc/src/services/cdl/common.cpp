#include <epoc/services/cdl/common.h>
#include <common/chunkyseri.h>

#include <algorithm>
#include <vector>

namespace eka2l1::epoc {
    void do_refs_state(common::chunkyseri &seri, cdl_ref_collection &collection_) {
        std::vector<std::u16string> names;

        for (std::size_t i = 0; i < collection_.size(); i++) {
            if (!std::binary_search(names.begin(), names.end(), collection_[i].name_)) {
                names.push_back(collection_[i].name_);
                std::sort(names.begin(), names.end());
            }
        }

        // Now serialize
        seri.absorb_container(names);
        seri.absorb_container(collection_, [&](common::chunkyseri &seri, cdl_ref &ref) {
            seri.absorb(ref.id_);
            seri.absorb(ref.uid_);

            auto name_idx = std::distance(names.begin(),
                std::lower_bound(names.begin(), names.end(), ref.name_));

            seri.absorb(name_idx);

            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                ref.name_ = names[name_idx];
            }
        });
    }
}
