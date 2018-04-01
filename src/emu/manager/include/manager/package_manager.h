#pragma once

#include <vector>
#include <string>
#include <map>

namespace eka2l1 {
    namespace manager {
        using uid = uint32_t;

        struct app_info {
            std::u16string name;
            std::u16string vendor_name;
            uint8_t drive;
        };

        // A package manager, serves for managing apps
        // and implements part of HAL
        class package_manager {
            std::map<uid, app_info> c_apps;
            std::map<uid, app_info> e_apps;

            bool load_sdb(const std::string& path);
            bool write_sdb(const std::string& path);

        public:
            package_manager() { load_sdb("apps_registry.sdb"); }

            bool installed(uid app_uid);

            std::u16string app_name(uid app_uid);

            bool install_package(const std::u16string& path, uint8_t drive);
            bool uninstall_package(uid app_uid);
        };
    }
}
