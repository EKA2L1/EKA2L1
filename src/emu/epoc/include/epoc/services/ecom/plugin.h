/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <common/types.h>
#include <epoc/loader/rsc.h>
#include <epoc/utils/dll.h>

#include <cstdint>
#include <vector>

namespace eka2l1 {
    namespace common {
        class chunkyseri;
    }

    struct ecom_implementation_info {
        std::u16string original_name;
        epoc::lib_info plugin_dll_info;
        
        std::uint32_t uid;
        std::uint8_t version;
        std::uint8_t format;

        std::u16string display_name;
        std::string default_data;
        std::string opaque_data;

        enum {
            FLAG_ROM = 1 << 0,
            FLAG_HINT_NO_EXTENDED_INTERFACE = 1 << 1,
            FLAG_IMPL_CREATE_INFO_CACHED = 1 << 2
        };

        std::uint32_t flags;

        drive_number drv; ///< Drive which the implementation plugin resides on

        // A implementation may covers other interface ?
        std::vector<std::uint32_t> extended_interfaces;

        void do_state(common::chunkyseri &seri, const bool support_extended_interface);
    };

    using ecom_implementation_info_ptr = std::shared_ptr<ecom_implementation_info>;

    struct ecom_interface_info {
        std::uint32_t uid;
        std::vector<ecom_implementation_info_ptr> implementations;
    };

    struct ecom_plugin {
        std::uint32_t type;
        std::uint32_t uid;

        std::vector<ecom_interface_info> interfaces;
    };

    enum ecom_type {
        ecom_plugin_type_2 = 0x101FB0B9,
        ecom_plugin_type_3 = 0x10009E47
    };

    /*
     * \brief Load a plugin from described resource file
     *
     * \returns True if success.
     */
    bool load_plugin(loader::rsc_file &rsc, ecom_plugin &plugin);
};
