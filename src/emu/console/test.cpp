#include <core/core_api.h>

#include <loader/rpkg.h>
#include <loader/sis_old.h>

int main() {
    int sys = create_symbian_system(GLFW, OPENGL, CPU_UNICORN);

    init_symbian_system(sys);

    mount_symbian_system(sys, "C:", "drives/c/");
    mount_symbian_system(sys, "E:", "drives/e/");
    mount_symbian_system(sys, "Z:", "drives/z/");

    install_rpkg(sys, "SYM.RPKG");

    shutdown_symbian_system(sys);
    free_symbian_system(sys);

    return 0;
}