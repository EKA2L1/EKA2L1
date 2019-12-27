/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace eka2l1::manager {
    enum class hal_entry_key {
        #define HAL_ENTRY(generic_name, display_name, key) generic_name = key,
        #include <manager/hal.def>
        #undef HAL_ENTRY
    };
    
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
        std::string cpu_backend { "unicorn" };
        int device { 0 };
        
        bool enable_gdbstub { false };
        int gdb_port { 24689 };

        std::string rom_path = "SYM.ROM";
        std::string storage = ".";      // Set this to dot, avoid making it absolute

        int display_size_x_pixs { 360 };
        int display_size_y_pixs { 640 };

        std::uint32_t maximum_ram;

        bool enable_srv_ecom {true};
        bool enable_srv_cenrep {true};
        bool enable_srv_backup {true};
        bool enable_srv_install {true};
        bool enable_srv_rights {true};
        bool enable_srv_sa { true };
        bool enable_srv_drm { true };
        bool enable_srv_eikapp_ui { true };
        bool enable_srv_akn_icon { true };
        bool enable_srv_akn_skin { true };
        bool enable_srv_cdl { true };

        void serialize();
        void deserialize();

        const std::uint32_t get_hal_entry(const int key) const;
    };
}
