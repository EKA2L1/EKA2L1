#include <manager/manager.h>
#include <manager/package_manager.h>

namespace eka2l1 {
    manager::package_manager *manager_system::get_package_manager() {
        return &pkgmngr;
    }

    void manager_system::init(io_system* ios) {
        io = ios;
        pkgmngr = manager::package_manager(ios);
    }
}
