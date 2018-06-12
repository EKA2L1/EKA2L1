#include <core/core_api.h>

int main() {
    int sys = create_symbian_system(GLFW, OPENGL, CPU_UNICORN);

    init_symbian_system(sys);

    mount_symbian_system(sys, "C:", "drives/c/");
    mount_symbian_system(sys, "E:", "drives/e/");

    install_sis(sys, 0, "nfs.sis");
    load_rom(sys, "SYM.ROM");

    /*
    load_process(sys, 0xECF52F7F);

    loop_system(sys);

	reinit_system(sys);
    load_process(sys, 0xECF52F7F);

    loop_system(sys);
    reinit_system(sys);
    load_process(sys, 0xECF52F7F);

    loop_system(sys);

    shutdown_symbian_system(sys);*/

    return 0;
}