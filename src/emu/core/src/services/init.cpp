#include <services/applist/applist.h>
#include <services/featmgr/featmgr.h>
#include <services/fontbitmap/fontbitmap.h>
#include <services/fs/fs.h>
#include <services/window/window.h>

#include <services/init.h>

#include <core.h>

#define CREATE_SERVER_D(sys, svr)                   \
    server_ptr temp = std::make_shared<##svr>(sys); \
    sys->get_kernel_system()->add_custom_server(temp)

#define CREATE_SERVER(sys, svr)          \
    temp = std::make_shared<##svr>(sys); \
    sys->get_kernel_system()->add_custom_server(temp)

#define DEFINE_INT_PROP_D(sys, category, key, data)                                                 \
    property_ptr prop = sys->get_kernel_system()->create_prop(service::property_type::int_data, 0); \
    prop->first = category;                                                                          \
    prop->second = key;                                                                              \
    prop->set(data);

#define DEFINE_INT_PROP(sys, category, key, data)                                                 \
    prop = sys->get_kernel_system()->create_prop(service::property_type::int_data, 0); \
    prop->first = category;                                                                         \
    prop->second = key;                                                                             \
    prop->set(data);

const uint32_t sys_category = 0x101f75b6;

const uint32_t hal_key_base = 0x1020e306;
const uint32_t unk_key1 = 0x1020e34e;

namespace eka2l1 {
    namespace service {
        void init_services(system *sys) {
            CREATE_SERVER_D(sys, applist_server);
            CREATE_SERVER(sys, featmgr_server);
            CREATE_SERVER(sys, fs_server);
            CREATE_SERVER(sys, fontbitmap_server);
            CREATE_SERVER(sys, window_server);

            // Unknown key, testing show that this prop return 65535 most of times
            // The prop belongs to HAL server, but the key usuage is unknown. (TODO)
            DEFINE_INT_PROP_D(sys, sys_category, unk_key1, 65535);   
        }
    }
}