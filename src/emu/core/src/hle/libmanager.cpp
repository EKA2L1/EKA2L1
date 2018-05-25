#include <hle/libmanager.h>
#include <common/log.h>
#include <yaml-cpp/yaml.h>

namespace eka2l1 {
    namespace hle {
        void lib_manager::load_all_sids(const epocver ver) {
			std::vector<sid> tids;
			std::string lib_name;

			#define LIB(x) lib_name = #x;
			#define EXPORT(x, y) tids.push_back(y); func_names.insert(std::make_pair(y, x)); 
			#define ENDLIB() ids.insert(std::make_pair(lib_name, tids)); tids.clear(); 

			if (ver == epocver::epoc6) {
				#include <hle/epoc6.h>
			} else {
				#include <hle/epoc9.h>
			}

			#undef LIB
			#undef EXPORT
			#undef ENLIB
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

        lib_manager::lib_manager(const epocver ver) {
            load_all_sids(ver);
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

        std::optional<sid> lib_manager::get_sid(exportaddr addr) {
            auto res = addr_map.find(addr);

            if (res == addr_map.end()) {
                return std::optional<sid>{};
            }

            return res->second;
        }
    }
}