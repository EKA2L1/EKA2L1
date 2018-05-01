#include <manager/manager.h>
#include <manager/package_manager.h>

namespace eka2l1 {
    namespace manager {
        package_manager mngr;

        package_manager *get_package_manager() {
            return &mngr;
        }
    }
}
