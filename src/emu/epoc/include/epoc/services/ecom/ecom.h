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

#include <epoc/services/server.h>

#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;

    enum ecom_opcodes {
        ecom_notify_on_change,
        ecom_cancel_notify_on_change,
        ecom_list_implementations,
        ecom_list_resolved_implementations,
        ecom_collect_implementations_list,
        ecom_get_implementaton_creation_method,
        ecom_get_resolved_creation_method,
        ecom_get_custom_resolved_creation_method,
        ecom_destroyed_implementation,
        ecom_enable_implementation,
        ecom_list_extended_interfaces,
        ecom_set_get_params
    };

    class ecom_server: public service::server {
    protected:
        /*! \brief Search the ROM and ROFS for an archive of plugins.
         *
         * Archive are SPI file. They usually has pattern of ecom-*-*.spi or
         * ecom-*-*.sXX where XX is language code.
         * 
         * This searchs these files on Z:\Private\10009d8f
         * 
         * \returns A vector contains all canidates.
        */
        std::vector<std::string> get_ecom_plugin_archives(eka2l1::io_system *io);

        /*! \brief Load archives
        */
        void load_archives();
        
    public:
        explicit ecom_server(eka2l1::system *sys);
    };
}