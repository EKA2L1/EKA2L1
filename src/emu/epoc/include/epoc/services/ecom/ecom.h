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

#include <epoc/services/ecom/plugin.h>
#include <epoc/services/server.h>
#include <epoc/utils/uid.h>

#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;

    namespace epoc::fs {
        struct entry;
    }

    enum ecom_opcodes {
        ecom_notify_on_change,
        ecom_cancel_notify_on_change,
        ecom_list_implementations,
        ecom_list_resolved_implementations,
        ecom_list_custom_resolved_implementations,
        ecom_collect_implementations_list,
        ecom_get_implementation_creation_method,
        ecom_get_resolved_creation_method,
        ecom_get_custom_resolved_creation_method,
        ecom_destroyed_implementation,
        ecom_enable_implementation,
        ecom_list_extended_interfaces,
        ecom_set_get_params
    };

    struct ecom_list_impl_param {
        std::int32_t match_type;
        std::int32_t buffer_size;
        std::int32_t cap_check;
    };

    class ecom_server : public service::server {
        std::unordered_map<std::uint32_t, ecom_interface_info> interfaces;

        // Storing implementations pointer for implementation look-up only. Costly
        // to use unordered_map.
        // We may need to reconsider this.
        std::vector<ecom_implementation_info_ptr> implementations;
        std::vector<ecom_implementation_info_ptr> collected_impls;

        bool init{ false };

        void do_get_resolved_impl_creation_method(service::ipc_context *ctx);
        bool unpack_match_str_and_extended_interfaces(std::string &data, std::string &match_str,
            std::vector<std::uint32_t> &extended_interfaces);

    protected:
        void list_implementations(service::ipc_context &ctx);
        void get_implementation_creation_method(service::ipc_context &ctx);
        void collect_implementation_list(service::ipc_context &ctx);

        bool get_implementation_buffer(std::uint8_t *buf, const std::size_t buf_size,
            const bool support_extended_interface);

        bool register_implementation(const std::uint32_t interface_uid,
            ecom_implementation_info_ptr &impl);

        bool load_plugins(eka2l1::io_system *io);
        bool load_and_install_plugin_from_buffer(const std::u16string &name, std::uint8_t *buf, const std::size_t size,
            const drive_number drv);

        bool load_plugin_on_drive(eka2l1::io_system *io, const drive_number drv);

        /*
         * \brief Search the ROM and ROFS for an archive of plugins.
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
        bool load_archives(eka2l1::io_system *io);

        void connect(service::ipc_context &ctx) override;

    public:
        explicit ecom_server(eka2l1::system *sys);

        /**
         * \brief Get interface info of a given UID.
         * \returns The pointer to the interface info, null means not found.
         */
        ecom_interface_info *get_interface(const epoc::uid interface_uid);

        bool get_implementation_dll_info(kernel::thread *requester, const epoc::uid interface_uid,
            const epoc::uid impl_uid, epoc::fs::entry &dll_entry, epoc::uid &dtor_key, std::int32_t *err,
            const bool check_cap_comp = true);
    };
}
