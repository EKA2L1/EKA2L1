#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace YAML {
	class Node;
}

namespace eka2l1 {
    using sid = uint32_t;
    using sids = std::vector<uint32_t>;
    using exportaddr = uint32_t;
    using exportaddrs = sids;

    typedef uint32_t address;

	enum class epocver {
		epoc6,
		epoc9
	};

    namespace hle {
        // This class is launched at configuration time, so
        // no race condition.
        class lib_manager {
            std::map<std::string, sids> ids;
			std::map<sid, std::string> func_names;

            std::map<std::string, exportaddrs> exports;

            std::map<address, sid> addr_map;

            void load_all_sids(const epocver ver);

        public:
            lib_manager(const epocver ver);
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