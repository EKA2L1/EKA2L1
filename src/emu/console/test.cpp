#include <core/core_api.h>
#include <loader/sis_old.h>

int main() {
    int sys = create_symbian_system(GLFW, OPENGL, CPU_UNICORN);

    init_symbian_system(sys);

    mount_symbian_system(sys, "C:", "drives/c/");
    mount_symbian_system(sys, "E:", "drives/e/");

    load_rom(sys, "SYM6.ROM");

    auto sis_file = eka2l1::loader::parse_sis_old("ENG2.sis");
    eka2l1::loader::sis_old of = sis_file.value();

    load_process(sys, 0xECF52F7F);

    loop_system(sys);

    reinit_system(sys);
    load_process(sys, 0xECF52F7F);

    loop_system(sys);
    reinit_system(sys);
    load_process(sys, 0xECF52F7F);

    loop_system(sys);

    shutdown_symbian_system(sys);

    return 0;
}