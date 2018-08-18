#include <core/epoc/panic.h>
#include <yaml-cpp/yaml.h>

#include <mutex>

namespace eka2l1::epoc {
    YAML::Node panic_node;
    std::mutex panic_mut;

    bool init_panic_descriptions() {
        std::lock_guard<std::mutex> guard(panic_mut);

        panic_node.reset();

        try {
            panic_node = YAML::LoadFile("panic.json")["Panic"];
        } catch (...) {
            return false;
        }

        return true;
    }

    bool is_panic_category_action_default(const std::string &panic_category) {
        try {
            const std::string action = panic_node[panic_category]["action"].as<std::string>();

            if (action == "script") {
                return false;
            }
        } catch (...) {
            return true;
        }

        return true;
    }

    std::optional<std::string> get_panic_description(const std::string &category, const int code) {
        try {
            const std::string description = panic_node[category][code].as<std::string>();
            return description;
        } catch (...) {
            return std::optional<std::string>{};
        }

        return std::optional<std::string>{};
    }
}