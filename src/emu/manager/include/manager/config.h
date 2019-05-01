#pragma once

#include <array>
#include <string>
#include <vector>
#include <optional>

namespace eka2l1::manager {
    struct config_state {
        float menu_height = 0;
        int bkg_transparency { 129 };
        std::string bkg_path;
        std::string font_path;

        bool log_read { false };
        bool log_write { false };
        bool log_svc { false };
        bool log_ipc { false };
        bool log_passed { false };
        bool log_exports { false };
        bool log_code { false };

        bool enable_breakpoint_script { false };

        std::vector<std::string> force_load_modules;
        int cpu_backend { 0 };
        int device { 0 };
        
        bool enable_gdbstub { false };
        int gdb_port { 24689 };

        std::string rom_path = "SYM.ROM";
        std::string c_mount = "drives/c/";
        std::string e_mount = "drives/e/";
        std::string z_mount = "drives/z/";

        void serialize();
        void deserialize();
    };
}
