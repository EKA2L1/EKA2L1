#include <epoc9/e32loader.h>
#include <loader/eka2img.h>

BRIDGE_FUNC(TInt, E32LoaderStaticCallList, TUint &aNumEps, address *aEpList) {
    hle::lib_manager *mngr = sys->get_lib_manager();
    auto cache_maps = mngr->get_e32imgs_cache();
    TUint count = aNumEps;
    TUint myCount = 0;

    if (count < 0) {
        return KErrGeneral;
    }

    address exe_addr = 0;

    for (auto cache : cache_maps) {
        if (count != 0xFFFFFFFF && myCount == count) {
            break;
        }
        
        auto res = std::find(cache.second.loader.begin(), cache.second.loader.end(), sys->get_kernel_system()->crr_process());

        // If this image is owned to the current process
        if (res != cache.second.loader.end()) {
            if (cache.second.img->header.uid1 == loader::eka2_img_type::exe) {
                exe_addr = cache.second.img->header.entry_point + cache.second.img->rt_code_addr;
            } else {
                aEpList[myCount] = cache.second.img->header.entry_point + cache.second.img->rt_code_addr;
                ++myCount;
            }
        }
    }

    if (exe_addr == 0)
        aNumEps = myCount;
    else {
        aNumEps = myCount + 1;
        aEpList[aNumEps - 1] = exe_addr;
    }

    return KErrNone;
}

const eka2l1::hle::func_map e32loader_register_funcs = {
};