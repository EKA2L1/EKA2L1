#include <loader/stub.h>
#include <yaml-cpp/yaml.h>

#include <mutex>

#ifdef _MSC_VER
#define __attribute__(x) 
#endif

#include <cxxabi.h>

namespace eka2l1 {
    namespace loader {
        YAML::Node db_node;
        YAML::Node meta_node;

        std::mutex mut;

		std::map<std::string, vtable_stub> vtable_cache;
		std::map<std::string, typeinfo_stub> typeinfo_cache;

		void function_stub::write_stub() {
			uint32_t* wh = reinterpret_cast<uint32_t*>(pos.get());

			wh[0] = 0xef000000; // svc #0
			wh[1] = 0xe1a0f00e;
			wh[2] = id;
		}

		void typeinfo_stub::write_stub() {
			uint32_t* wh = reinterpret_cast<uint32_t*>(pos.get());
			uint8_t* wh8 = reinterpret_cast<uint8_t*>(pos.get());

			wh[0] = helper.ptr_address();
			wh8 += 4;
			
			// I will provide a fake abi class, and there is nothing to hide, so ...
			memcpy(wh8, info_des.data(), info_des.length());

			++wh8 = '\0';
			wh = reinterpret_cast<uint32_t*>(wh8);

			wh[0] = parent_info.ptr_address();
		}

		void write_non_virtual_thunk(uint32_t* pos, const uint32_t virt_virt_func, uint32_t top_off) {
			pos[0] = 0x4883ef00 | top_off; // sub #top_off, %?
			pos[1] = 0xeb000000; // jmp virt_virt_func
			pos[2] = virt_virt_func;
		}

		void vtable_stub::write_stub() {
			uint32_t* wh = reinterpret_cast<uint32_t*>(pos.get());
			auto bases = parent->bases;

			// UNFINISHED
		}

        // Extract the library function from the database, then write the stub into memory
        module_stub write_module_stub(std::string name, const std::string &db_path) {
            db_node = YAML::LoadFile(db_path + "/db.yml");
            meta_node = YAML::LoadFile(db_path + "/meta.yml");

            auto all_stubs = db_node[name.c_str()];

			return module_stub{};
        }
    }
}
