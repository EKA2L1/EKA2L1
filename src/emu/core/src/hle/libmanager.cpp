#include <hle/libmanager.h>

namespace eka2l1 {
    namespace hle {
        void lib_manager::load_all_sids() {
            for (auto lib: root) {
                std::string lib_name = lib.first.as<std::string>();
                std::vector<sid> tids;

                for (auto lib_entry: lib.second) {
                    tids.push_back(lib_entry.second.as<uint32_t>());
                }

                ids.insert(std::make_pair(lib_name, tids));
            }
        }

        std::optional<sids> lib_manager::get_sids(const std::string& lib_name) {
            auto res = ids.find(lib_name);

            if (res == ids.end()) {
                return std::optional<sids>{};
            }

            return res->second;
        }

        std::optional<exportaddrs> lib_manager::get_export_addrs(const std::string& lib_name) {
            auto res = exports.find(lib_name);

            if (res == exports.end()) {
                return std::optional<exportaddrs>{};
            }

            return res->second;
        }

        lib_manager::lib_manager(const std::string db_path) {
            root = YAML::LoadFile(db_path)["libraries"];
            load_all_sids();
        }

        bool lib_manager::register_exports(const std::string& lib_name, exportaddrs& addrs) {           
            auto libidsop = get_sids(lib_name);

            if (!libidsop) {
                return false;
            }

            exports.insert(std::make_pair(lib_name, addrs));

            sids libids = libidsop.value();

            for (uint32_t i = 0; i < addrs.size(); i++) {
                addr_map.insert(std::make_pair(addrs[i], libids[i]));
            }

            return true;
        }

        std::optional<sid> lib_manager::get_sid(exportaddress addr) {
            auto res = addr_map.find(addr);

            if (res == addr_map.end()) {
                return std::optional<sid>{};
            }

            return res->second;
        }
    }
}