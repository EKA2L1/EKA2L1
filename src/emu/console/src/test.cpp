#include <core/core.h>
#include <common/log.h>

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);

    eka2l1::system sys(nullptr, nullptr);
    sys.init();

    sys.load_rom("SYM.ROM");
    sys.mount(drive_c, eka2l1::drive_media::physical, "drives/c/", eka2l1::io_attrib::internal);
    sys.mount(drive_e, eka2l1::drive_media::physical, "drives/e/", eka2l1::io_attrib::removeable);
    sys.mount(drive_z, eka2l1::drive_media::rom,
        "drives/z/", eka2l1::io_attrib::internal | eka2l1::io_attrib::write_protected);

    sys.get_lib_manager()->init(&sys, sys.get_kernel_system(),
        sys.get_io_system(), sys.get_memory_system(), epocver::epoc9);

    eka2l1::loader::e32img_ptr img = 
        sys.get_lib_manager()->load_e32img(u"mpxplaylistrecognizer.dll");

    sys.get_lib_manager()->open_e32img(img);

    eka2l1::loader::e32img_ptr imgcommon = 
        sys.get_lib_manager()->load_e32img(u"mpxcommon.dll");

    // Test library address
    std::uint32_t lib_handle = sys.get_kernel_system()->create_library("Test", imgcommon, 
        eka2l1::kernel::owner_type::kernel);

    eka2l1::library_ptr lib = std::dynamic_pointer_cast<eka2l1::kernel::library>
        (sys.get_kernel_system()->get_kernel_obj(lib_handle));

    auto att = lib->attach();
    auto addr = lib->get_ordinal_address(1);

    sys.shutdown();
    return 0;
}