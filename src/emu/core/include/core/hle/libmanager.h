#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

#include <yaml-cpp/yaml.h>

namespace eka2l1 {
    using sid = uint32_t;

    namespace hle {
        class lib_manager {
            using sids = std::vector<uint32_t>;
            std::map<std::string, sids> ids;
            std::map<sid, void*> registered;

            YAML::Node root;

            void load_all_sids();

        public:
            lib_manager(const std::string db_path);
            std::optional<sids> get_sids(const std::string& lib_name);
        };
    }
}