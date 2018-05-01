#pragma once

#include <string>

namespace eka2l1 {
    namespace vfs {
        void init();
        void shutdown();

        std::string current_dir();
        void current_dir(const std::string &new_dir);

        void mount(const std::string &dvc, const std::string &real_path);
        void unmount(const std::string &dvc);

        // Map a virtual path to real path
        std::string get(std::string vir_path);
    }
}
