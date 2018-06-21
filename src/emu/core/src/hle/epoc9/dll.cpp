#include <epoc9/dll.h>
#include <loader/eka2img.h>
#include <loader/romimage.h>

using namespace eka2l1;

eka2l1::kernel::tls_slot *get_tls_slot(eka2l1::system *sys, address addr) {
    hle::lib_manager *mngr = sys->get_lib_manager();
    auto &rommap = mngr->get_romimgs_cache();

    auto &dll = std::find_if(rommap.begin(), rommap.end(),
        [addr](auto &rom) {
            loader::romimg_ptr romimg = rom.second.img;
            loader::rom_image_header header = romimg->header;

            if (header.code_address <= addr && addr <= header.code_address + header.code_size) {
                return true;
            }

            return false;
        });

    if (dll != rommap.end()) {
        return sys->get_kernel_system()->crr_thread()->get_free_tls_slot(addr, dll->second.img->header.code_address);
    }

    auto &imgmap = mngr->get_e32imgs_cache();

    auto &edll = std::find_if(imgmap.begin(), imgmap.end(),
        [addr](auto &eimg) {
            loader::e32img_ptr img = eimg.second.img;
            loader::eka2img_header header = img->header;

            if (img->rt_code_addr < addr && addr < img->rt_code_addr + header.code_size) {
                return true;
            }

            return false;
        });

    if (edll != imgmap.end()) {
        return sys->get_kernel_system()->crr_thread()->get_free_tls_slot(addr, edll->second.img->rt_code_addr);
    }

    return nullptr;
}
