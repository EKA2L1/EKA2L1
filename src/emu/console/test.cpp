#include <core/core_api.h>
#include <loader/sis_old.h>

int main() {
    int sys = create_symbian_system(GLFW, OPENGL, CPU_UNICORN);

    init_symbian_system(sys);

    mount_symbian_system(sys, "C:", "drives/c/");
    mount_symbian_system(sys, "E:", "drives/e/");

    load_rom(sys, "SYM6.ROM");

    install_sis(sys, 0, "floatingduck_gcce.sisx");

    shutdown_symbian_system(sys);
    free_symbian_system(sys);

    return 0;
}