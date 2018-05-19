#pragma once

#include <map>
#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;

    namespace loader {
        class sis_controller;
    }

    namespace manager {
        using uid = uint32_t;

        struct app_info {
            std::u16string name;
            std::u16string vendor_name;
            std::u16string executable_name;
            uint8_t drive;
        };

        // A package manager, serves for managing apps
        // and implements part of HAL
        class package_manager {
            std::map<uid, app_info> c_apps;
            std::map<uid, app_info> e_apps;

            bool load_sdb(const std::string &path);
            bool write_sdb(const std::string &path);

            io_system* io;

            bool install_controller(loader::sis_controller *ctrl, uint8_t drv);

        public:
            package_manager() = default;
            package_manager(io_system* io): io(io) { load_sdb("apps_registry.sdb"); }

            bool installed(uid app_uid);

            std::u16string app_name(uid app_uid);
            app_info info(uid app_uid);

            bool install_package(const std::u16string &path, uint8_t drive);
            bool uninstall_package(uid app_uid);
        };
    }
}
