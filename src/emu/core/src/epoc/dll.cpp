#include <epoc/dll.h>
#include <core.h>

namespace eka2l1::epoc {
    std::vector<uint32_t> query_entries(eka2l1::system *sys) {
        hle::lib_manager &mngr = *sys->get_lib_manager();
        std::vector<uint32_t> entries;
        
        auto cache_maps = mngr.get_e32imgs_cache();
        address exe_addr = 0;

        for (auto &cache : cache_maps) {
            auto res = std::find(cache.second.loader.begin(), cache.second.loader.end(), sys->get_kernel_system()->crr_process());

            // If this image is owned to the current process
            if (res != cache.second.loader.end()) {
                if (cache.second.img->header.uid1 == loader::eka2_img_type::exe) {
                    exe_addr = cache.second.img->header.entry_point + cache.second.img->rt_code_addr;
                } else {
                    entries.push_back(cache.second.img->header.entry_point + cache.second.img->rt_code_addr);
                }
            }
        }

        if (exe_addr != 0) {
            entries.push_back(exe_addr);
        }

        return entries;
    }
}