#pragma once

#include <common/types.h>
#include <manager/package_manager.h>

#include <cstdint>
#include <string>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::loader {
    bool install_sis_old(std::u16string path, io_system *io, drive_number drive, epocver sis_ver, manager::app_info &info);
}