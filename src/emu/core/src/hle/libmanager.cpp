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

        std::optional<lib_manager::sids> lib_manager::get_sids(const std::string& lib_name) {
            auto res = ids.find(lib_name);

            if (res == ids.end()) {
                return std::optional<sids>{};
            }

            return res->second;
        }

        lib_manager::lib_manager(const std::string db_path) {
            root = YAML::LoadFile(db_path)["libraries"];
            load_all_sids();
        }
    }
}