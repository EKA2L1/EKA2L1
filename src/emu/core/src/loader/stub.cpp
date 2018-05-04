#include <loader/stub.h>
#include <yaml-cpp/yaml.h>

#include <mutex>

namespace eka2l1 {
    namespace loader {
        YAML::Node db_node;
        YAML::Node meta_node;

        std::mutex mut;

        // Extract the library function from the database, then write the stub into memory
        module_stub write_module_stub(std::string name, const std::string &db_path) {
            db_node = YAML::LoadFile(db_path + "/db.yml");
            meta_node = YAML::LoadFile(db_path + "/meta.yml");

            stub lib_stub;
            auto all_stubs = db_node[name.c_str()];
        }
    }
}
