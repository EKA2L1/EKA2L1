#include <io/vfs.h>
#include <loader/sis.h>
#include <iostream>

using namespace eka2l1;

int main(int argc, char** argv) {
    auto success = loader::install_sis("/home/dtt2502/Miscs/super_miners.sis", loader::sis_drive::drive_c);
}
