#include <services/applist/applist.h>
#include <services/featmgr/featmgr.h>
#include <services/fontbitmap/fontbitmap.h>
#include <services/fs/fs.h>
#include <services/window/window.h>

#include <services/init.h>

#include <core.h>

#include <e32lang.h>

#define CREATE_SERVER_D(sys, svr)                   \
    server_ptr temp = std::make_shared<##svr>(sys); \
    sys->get_kernel_system()->add_custom_server(temp)

#define CREATE_SERVER(sys, svr)          \
    temp = std::make_shared<##svr>(sys); \
    sys->get_kernel_system()->add_custom_server(temp)

#define DEFINE_INT_PROP_D(sys, category, key, data)                                                     \
    property_ptr prop = sys->get_kernel_system()->create_prop(service::property_type::int_data, 0); \
    prop->first = category;                                                                         \
    prop->second = key;                                                                             \
    prop->set(data);

#define DEFINE_INT_PROP(sys, category, key, data)                                          \
    prop = sys->get_kernel_system()->create_prop(service::property_type::int_data, 0); \
    prop->first = category;                                                            \
    prop->second = key;                                                                \
    prop->set(data);

#define DEFINE_BIN_PROP_D(sys, category, key, size, data)                                                 \
    property_ptr prop = sys->get_kernel_system()->create_prop(service::property_type::bin_data, size); \
    prop->first = category;                                                                         \
    prop->second = key;                                                                             \
    prop->set(data);

#define DEFINE_BIN_PROP(sys, category, key, size, data)                                      \
    prop = sys->get_kernel_system()->create_prop(service::property_type::bin_data, size); \
    prop->first = category;                                                            \
    prop->second = key;                                                                \
    prop->set(data);

const uint32_t sys_category = 0x101f75b6;

const uint32_t hal_key_base = 0x1020e306;
const uint32_t unk_key1 = 0x1020e34e;

const uint32_t locale_data_key = 0x10208904;
const uint32_t locale_lang_key = 0x10208903;

namespace epoc {
    struct TLocale {
    };

    struct SLocaleLanguage {
        TLanguage iLanguage;
        eka2l1::ptr<char> iDateSuffixTable;
        eka2l1::ptr<char> iDayTable;
        eka2l1::ptr<char> iDayAbbTable;
        eka2l1::ptr<char> iMonthTable;
        eka2l1::ptr<char> iMonthAbbTable;
        eka2l1::ptr<char> iAmPmTable;
        eka2l1::ptr<uint16_t> iMsgTable;
    };
}

namespace eka2l1 {
    namespace service {
        void init_services(system *sys) {
            CREATE_SERVER_D(sys, applist_server);
            CREATE_SERVER(sys, featmgr_server);
            CREATE_SERVER(sys, fs_server);
            CREATE_SERVER(sys, fontbitmap_server);
            CREATE_SERVER(sys, window_server);

            auto lang = epoc::SLocaleLanguage{ TLanguage::ELangEnglish, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

            // Unknown key, testing show that this prop return 65535 most of times
            // The prop belongs to HAL server, but the key usuage is unknown. (TODO)
            DEFINE_INT_PROP_D(sys, sys_category, unk_key1, 65535);
            DEFINE_BIN_PROP(sys, sys_category, locale_lang_key, sizeof(epoc::SLocaleLanguage), lang);
        }
    }
}