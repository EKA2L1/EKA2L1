#include <core/core_api.h>

int main() {
    int sys = create_symbian_system(GLFW, OPENGL, CPU_UNICORN);

    init_symbian_system(sys);

    mount_symbian_system(sys, VFS_ATTRIB_INTERNAL, VFS_MEDIA_PHYSICAL, VFS_DRIVE_C, "drives/c/");
    mount_symbian_system(sys, VFS_ATTRIB_REMOVEABLE, VFS_MEDIA_PHYSICAL, VFS_DRIVE_E, "drives/e/");
    mount_symbian_system(sys, VFS_ATTRIB_INTERNAL | VFS_ATTRIB_WRITE_PROTECTED , VFS_MEDIA_ROM, VFS_DRIVE_Z, "drives/z/");

    install_sis(sys, 0, "example.sis");

    shutdown_symbian_system(sys);
    free_symbian_system(sys);

    return 0;
}