#pragma once

#include <manager/package_manager.h>

namespace eka2l1 {
    class manager_system {
        manager::package_manager pkgmngr;
        io_system *io;

    public:
        void init(io_system *ios);
        manager::package_manager *get_package_manager();
    };
}
