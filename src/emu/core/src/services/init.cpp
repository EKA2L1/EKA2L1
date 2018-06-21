#include <services/applist/applist.h>
#include <services/init.h>

#include <core.h>

#define CREATE_SERVER_D(sys, svr)          \
    server_ptr temp = std::make_shared<##svr>(sys); \
    sys->get_kernel_system()->add_custom_server(temp)

#define CREATE_SERVER(sys, svr)          \
    temp = std::make_shared<##svr>(sys); \
    sys->get_kernel_system()->add_custom_server(temp)

namespace eka2l1 {
    namespace service {
        void init_services(system *sys) {
            CREATE_SERVER_D(sys, applist_server);
        }
    }
}