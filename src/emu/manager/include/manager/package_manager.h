#pragma once

#include <vector>
#include <string>
#include <map>

namespace eka2l1 {
    namespace manager {
        using uid = uint32_t;

        // A package manager, serves for managing apps
        // and implements part of HAL
        class package_manager {
            std::vector<std::string> search_dirs;
            std::map<uid, std::u16string> apps;

        public:
            package_manager();

            void add_search_path(const std::string& path);
            bool installed(uid app_uid);

            std::u16string app_name(uid app_uid);

            bool install_package(const std::wstring& path, uint8_t drive);
        };
    }
}
