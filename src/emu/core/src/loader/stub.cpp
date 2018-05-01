#include <loader/stub.h>
#include <yaml-cpp/yaml.h>

#include <mutex>

namespace eka2l1 {
    namespace loader {
        YAML::Node db_node;
        YAML::Node meta_node;

        std::mutex mut;
    }
}
