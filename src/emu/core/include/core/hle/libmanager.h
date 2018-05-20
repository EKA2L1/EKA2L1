#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

#include <yaml-cpp/yaml.h>

namespace eka2l1 {
    using sid = uint32_t;
    using sids = std::vector<uint32_t>;
    using exportaddr = uint32_t;
    using exportaddrs = sids;
    
    typedef uint32_t address;

    namespace hle {
        // This class is launched at configuration time, so
        // no race condition.
        class lib_manager {
            std::map<std::string, sids> ids;
            std::map<std::string, exportaddrs> exports;

            std::map<address, sid> addr_map;

            YAML::Node root;

            void load_all_sids();

        public:
            lib_manager(const std::string db_path);
            std::optional<sids> get_sids(const std::string& lib_name);
            std::optional<exportaddrs> get_export_addrs(const std::string& lib_name);

            // Register export addresses for desired HLE library
            // This will also map the export address with the correspond SID
            // Note that these export addresses are unique, since they are the address in
            // the memory.
            bool register_exports(const std::string& lib_name, exportaddrs& addrs);
            std::optional<sid> get_sid(exportaddr addr);
        };
    }
}