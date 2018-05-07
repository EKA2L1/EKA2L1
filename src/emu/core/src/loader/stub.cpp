#include <loader/stub.h>
#include <yaml-cpp/yaml.h>
#include <core_mem.h>

//#include <cxxabi.h>
#include <cstring>
#include <mutex>

namespace eka2l1 {
    namespace loader {
        YAML::Node db_node;
        YAML::Node meta_node;

        std::mutex mut;
        std::map<sid, vtable_stub> vtable_caches;

        void function_stub::write_stub() {
            uint32_t* writepos = reinterpret_cast<uint32_t*>(pos.get());

            writepos[0] = 0xef000000;
            writepos[1] = 0xe1a0f00e; 
            writepos[2] = id;
        }

        // First, write the position of the helper vtable (abi::__cxx_class_helper)
        // Then, write the typeinfo name. This is usually the class
        void typeinfo_stub::write_stub() {
            uint8_t* stub8 = reinterpret_cast<uint8_t*>(pos.get());
            uint32_t* stub32 = reinterpret_cast<uint32_t*>(pos.get());
        
            stub32[0] = helper.ptr_address(); 
            stub8 += 4;

            // There is oviviously nothing to hide, and i bet no game
            // use this
            memcpy(stub8, info_des.data(), info_des.length());

            stub8[info_des.length()] = '\0';
            stub8 += info_des.length() + 1;

            stub32 = reinterpret_cast<uint32_t*>(stub8);
            stub32[0] = parent.ptr_address();
        }

        void vtable_stub::write_stub() {
            
        }

        // Extract the library function from the database, then write the stub into memory
        module_stub write_module_stub(std::string name, const std::string &db_path) {
            db_node = YAML::LoadFile(db_path + "/db.yml");
            meta_node = YAML::LoadFile(db_path + "/meta.yml");

            auto all_exports = db_node[name.c_str()];

            return module_stub{};
        }
    }
}
