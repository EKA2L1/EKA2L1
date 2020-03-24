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

#include <cassert>
#include <regex>

#include <common/buffer.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/path.h>
#include <common/log.h>

#include <epoc/services/ecom/ecom.h>
#include <epoc/vfs.h>

#include <epoc/epoc.h>
#include <epoc/loader/spi.h>
#include <epoc/utils/uid.h>

#include <epoc/services/ecom/common.h>
#include <common/wildcard.h>
#include <epoc/utils/err.h>

namespace eka2l1 {
    bool ecom_server::register_implementation(const std::uint32_t interface_uid,
        ecom_implementation_info_ptr &impl) {
        auto &interface = interfaces[interface_uid];

        if (!std::binary_search(interface.implementations.begin(), interface.implementations.end(), impl,
            [&](const ecom_implementation_info_ptr &impl1, const ecom_implementation_info_ptr &impl2) { return impl1->uid < impl2->uid; })) {
            interface.implementations.push_back(impl);
            implementations.push_back(impl);

            // Sort
            std::sort(interface.implementations.begin(), interface.implementations.end(),
                [](const ecom_implementation_info_ptr &lhs, const ecom_implementation_info_ptr &rhs) {
                    return lhs->uid < rhs->uid;
                });
            
            std::sort(implementations.begin(), implementations.end(),
                [](const ecom_implementation_info_ptr &lhs, const ecom_implementation_info_ptr &rhs) {
                    return lhs->uid < rhs->uid;
                });

            return true;
        }

        return false;
    }

    std::vector<std::string> ecom_server::get_ecom_plugin_archives(eka2l1::io_system *io) {
        std::u16string pattern = u"";

        // Get ROM drive first
        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
            auto res = io->get_drive_entry(drv);
            if (res && res->media_type == drive_media::rom) {
                pattern += drive_to_char16(drv);
            }
        }

        if (pattern.empty()) {
            // For some reason, ROM hasn't been mounted yet, break!
            return {};
        }

        pattern += u":\\Private\\10009d8f\\ecom-*-*.s*";
        auto ecom_private_dir = io->open_dir(pattern, io_attrib::none);

        if (!ecom_private_dir) {
            // Again folder not found or error from our side
            LOG_ERROR("Private directory of ecom can't be accessed");
            return {};
        }

        std::vector<std::string> results;

        while (auto entry = ecom_private_dir->get_next_entry()) {
            results.push_back(entry->full_path);
        }

        return results;
    }

    bool ecom_server::load_and_install_plugin_from_buffer(const std::u16string &name, std::uint8_t *buf, const std::size_t size,
        const drive_number drv) {
        common::ro_buf_stream stream(buf, size);
        loader::rsc_file rsc(reinterpret_cast<common::ro_stream*>(&stream));

        ecom_plugin plugin;

        bool result = load_plugin(rsc, plugin);

        if (!result) {
            return false;
        }

        for (auto &pinterface : plugin.interfaces) {
            // Get from the current interface on server
            auto &interface_on_server = interfaces[pinterface.uid];
            interface_on_server.uid = pinterface.uid;

            for (auto &impl : pinterface.implementations) {
                impl->drv = drv;
                impl->original_name = eka2l1::replace_extension(eka2l1::filename(name), u"");

                if (impl->original_name.back() == u'\0') {
                    impl->original_name.pop_back();
                }

                if (!register_implementation(pinterface.uid, impl)) {
                    return false;
                }
            }

            pinterface.implementations.clear();
        }

        return true;
    }

    ecom_interface_info *ecom_server::get_interface(const epoc::uid interface_uid) {
        if (!init) {
            if (!load_plugins(sys->get_io_system())) {
                LOG_ERROR("An error happens with initialization of ECom");
            }

            init = true;
        }

        // First, lookup the interface
        auto interface_ite = interfaces.find(interface_uid);

        // We can't find the interface!!
        if (interface_ite == interfaces.end()) {
            return nullptr;
        }

        return &interface_ite->second;
    }

    bool ecom_server::load_archives(eka2l1::io_system *io) {
        std::vector<std::string> archives = get_ecom_plugin_archives(io);

        for (const std::string &archive : archives) {
            const drive_number drv = char16_to_drive(archive[0]);

            symfile f = io->open_file(common::utf8_to_ucs2(archive), READ_MODE | BIN_MODE);
            std::vector<std::uint8_t> buf;
            buf.resize(f->size());

            f->read_file(&buf[0], static_cast<std::uint32_t>(buf.size()), 1);
            f->close();

            common::chunkyseri seri(buf.data(), buf.size(), common::SERI_MODE_READ);
            loader::spi_file spi(0);

            bool result = spi.do_state(seri);

            if (!result) {
                LOG_TRACE("SPI file {} corrupted!", archive);
                return false;
            }

            for (auto &entry : spi.entries) {
                result = load_and_install_plugin_from_buffer(
                    common::utf8_to_ucs2(entry.name), &entry.file[0], entry.file.size(), drv);

                if (!result) {
                    LOG_WARN("Can't load and install plugin \"{}\"", entry.name);
                }
            }
        }

        return true;
    }

    bool ecom_server::load_plugins(eka2l1::io_system *io) {
        // Load archives first
        if (!load_archives(io)) {
            return false;
        }

        for (drive_number drv = drive_a; drv <= drive_z; drv = (drive_number)((int)drv + 1)) {
            if (io->get_drive_entry(drv)) {
                // Load and ignore invalid plugins
                load_plugin_on_drive(io, drv);
            }
        }

        return true;
    }

    bool ecom_server::load_plugin_on_drive(eka2l1::io_system *io, const drive_number drv) {
        // Opening directory
        std::u16string plugin_dir_path;
        plugin_dir_path += drive_to_char16(drv);
        plugin_dir_path += u":\\Resource\\Plugins\\*.r*";

        auto plugin_dir = io->open_dir(plugin_dir_path, io_attrib::none);
        if (!plugin_dir) {
            LOG_TRACE("Plugins directory for drive {} not found!",
                static_cast<char>(plugin_dir_path[0]));

            return false;
        }

        while (auto entry = plugin_dir->get_next_entry()) {
            symfile f = io->open_file(common::utf8_to_ucs2(entry->full_path), READ_MODE | BIN_MODE);

            assert(f);

            std::vector<std::uint8_t> dat;
            dat.resize(f->size());
            f->read_file(&dat[0], static_cast<std::uint32_t>(dat.size()), 1);
            f->close();

            if (!load_and_install_plugin_from_buffer(common::utf8_to_ucs2(entry->full_path), &dat[0], dat.size(), drv)) {
                LOG_ERROR("Can't load and install plugins description {}", entry->name);
                return false;
            }
        }

        // TODO: Register a notification to the server side

        return true;
    }

    void ecom_server::connect(service::ipc_context &ctx) {
        if (!init) {
            if (!load_plugins(ctx.sys->get_io_system())) {
                LOG_ERROR("An error happens with initialization of ECom");
            }
        }

        init = true;
        ctx.set_request_status(epoc::error_none);
    }

    bool ecom_server::get_implementation_buffer(std::uint8_t *buf, const std::size_t buf_size,
        const bool support_extended_interface) {
        if (buf == nullptr) {
            return false;
        }

        common::chunkyseri seri(buf, buf_size, common::SERI_MODE_WRITE);

        std::uint32_t total_impls = static_cast<std::uint32_t>(collected_impls.size());
        seri.absorb(total_impls);

        for (auto &impl : collected_impls) {
            if (seri.eos()) {
                return false;
            }

            impl->do_state(seri, support_extended_interface);
        }

        return true;
    }

    bool ecom_server::unpack_match_str_and_extended_interfaces(std::string &data, std::string &match_str, 
        std::vector<std::uint32_t> &extended_interfaces) {
        // Data is empty. No match string, no extended interfaces whatsoever
        if (data.length() == 0) {
            extended_interfaces.resize(0);
            match_str = "";

            return true;
        }

        common::ro_buf_stream arg2_stream(reinterpret_cast<std::uint8_t *>(&data[0]),
            data.length());

        int len = 0;

        if (arg2_stream.read(&len, 4) < 4) {
            return false;
        }

        match_str.resize(len);

        if (arg2_stream.read(&match_str[0], len) < len) {
            return false;
        }

        // Now read all extended interfaces
        if (auto total_read = arg2_stream.read(&len, 4) < 4) {
            // No extended interfaces. Probably old version, so just ignore it.
            if (total_read == 0) {
                extended_interfaces.resize(0);
                return true;
            }

            return false;
        }

        extended_interfaces.resize(len);

        for (auto &extended_interface : extended_interfaces) {
            if (arg2_stream.read(&extended_interface, 4) < 4) {
                return false;
            }
        }

        return true;
    }
        
    void ecom_server::list_implementations(service::ipc_context &ctx) {
        // Clear last cache
        collected_impls.clear();

        // TODO: See if there is any exception (eg N97 could support it, not sure).
        const bool support_extended_interface = 
            ctx.sys->get_symbian_version_use() > epocver::epoc94;

        // The UID type is the first parameter
        // First UID contains the interface UID, while the third one contains resolver UID
        // The middle UID is reserved
        epoc::uid_type uids;

        // Use chunkyseri

        if (auto uids_result = ctx.get_arg_packed<epoc::uid_type>(0)) {
            uids = std::move(uids_result.value());
        } else {
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        std::string match_str;
        std::vector<std::uint32_t> given_extended_interfaces;

        // I only want to do reading in a scope since these will be useless really soon
        {
            // The second IPC argument contains the name match string and extend interface list
            // Let's do some reading
            std::string arg2_data;

            if (auto arg2_data_op = ctx.get_arg<std::string>(1)) {
                arg2_data = std::move(arg2_data_op.value());
            } else {
                ctx.set_request_status(epoc::error_argument);
                return;
            }

            if (!support_extended_interface) {
                match_str = arg2_data;
            } else {
                if (!unpack_match_str_and_extended_interfaces(arg2_data, match_str, given_extended_interfaces)) {
                    // TODO: This is some mysterious here.
                    ctx.set_request_status(epoc::error_argument);
                    return;
                }
            }
        }

        // Get list implementations extra parameters
        ecom_list_impl_param list_impl_param;
        if (auto list_impl_param_op = ctx.get_arg_packed<ecom_list_impl_param>(2)) {
            list_impl_param = std::move(list_impl_param_op.value());
        } else {
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        // We still gonna list all of implementation into a global buffer
        // So that when the guest buffer is not large enough, the server buffer will still store
        // all the implementations collected, and ECollectImplementationLists will collect it later
        // List it first, then write!

        // An implementation is considered to be accepted through these conditions:
        // - All extended interfaces given are available in the implementation
        // - Match the wildcard (if wildcard not empty)

        // May want to construct a regex for name comparing if we use generic match
        std::regex wildcard_matcher;

        if (list_impl_param.match_type && !match_str.empty()) {
            wildcard_matcher = std::move(std::regex(common::wildcard_to_regex_string(match_str)));
        }

        // First, lookup the interface
        ecom_interface_info *interface = get_interface(uids.uid1);

        if (!interface) {
            ctx.set_request_status(epoc::ecom_error_code::ecom_no_interface_identified);
            return;
        }

        // Open a serializer for measure data size
        common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);

        {
            std::uint32_t total_impls = 0;

            // Absorb padding
            seri.absorb(total_impls);
        }

        // Iterate thorugh all implementations
        for (ecom_implementation_info_ptr &implementation : interface->implementations) {
            // Check the extended interfaces first
            bool sastify = true;

            for (std::uint32_t &given_extended_interface : given_extended_interfaces) {
                if (std::lower_bound(implementation->extended_interfaces.begin(), implementation->extended_interfaces.end(),
                        given_extended_interface)
                    == implementation->extended_interfaces.end()) {
                    sastify = false;
                    break;
                }
            }

            // Not sastify ? Yeah, let's just turn back
            if (!sastify) {
                continue;
            }

            // Match string empty, so we should add into nice stuff now
            if (match_str.empty()) {
                collected_impls.push_back(implementation);
                implementation->do_state(seri, support_extended_interface);
                continue;
            }

            sastify = false;

            // We still need to see if the name is match
            // Generic match ? Wildcard check
            if (list_impl_param.match_type) {
                if (std::regex_match(implementation->default_data, wildcard_matcher)) {
                    sastify = true;
                }
            } else {
                if (implementation->default_data.find(match_str) != std::string::npos) {
                    sastify = true;
                }
            }

            // TODO: Capability supply

            if (sastify) {
                collected_impls.push_back(implementation);
                implementation->do_state(seri, support_extended_interface);
            }
        }

        // Compare the total buffer size we are going to write with the one guest provided
        // If it's not sufficient enough, throw epoc::error_overflow
        const std::size_t total_buffer_size_require = seri.size();
        const std::size_t total_buffer_size_given = list_impl_param.buffer_size;

        list_impl_param.buffer_size = static_cast<const int>(total_buffer_size_require);

        if (total_buffer_size_require > total_buffer_size_given) {
            // Write new list impl param, which contains the required new size
            ctx.write_arg_pkg<ecom_list_impl_param>(2, list_impl_param, nullptr, true);
            ctx.set_request_status(epoc::error_overflow);

            return;
        }

        // Write the buffer
        // This must not fail
        if (!get_implementation_buffer(ctx.get_arg_ptr(3), total_buffer_size_given, support_extended_interface)) {
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        // Write list implementation param to tell the client how much bytes we actually write
        ctx.write_arg_pkg<ecom_list_impl_param>(2, list_impl_param, nullptr, true);

        // Set the buffer length. It's not I like it or anything, baka
        ctx.set_arg_des_len(3, static_cast<const std::uint32_t>(total_buffer_size_require));

        // Finally, returns
        ctx.set_request_status(epoc::error_none);
    }

    void ecom_server::get_implementation_creation_method(service::ipc_context &ctx) {
        do_get_resolved_impl_creation_method(&ctx);
    }

    void ecom_server::collect_implementation_list(service::ipc_context &ctx) {
        const bool support_extended_interface = 
            ctx.sys->get_symbian_version_use() > epocver::epoc94;

        const std::size_t total_buffer_size_given = ctx.get_arg_max_size(0);

        if (!get_implementation_buffer(ctx.get_arg_ptr(0), total_buffer_size_given, support_extended_interface)) {
            ctx.set_request_status(epoc::error_overflow);
            return;
        }

        ctx.set_arg_des_len(0, static_cast<std::uint32_t>(total_buffer_size_given));

        collected_impls.clear();
        ctx.set_request_status(epoc::error_none);
    }
        
    ecom_server::ecom_server(eka2l1::system *sys)
        : service::server(sys, "!ecomserver", true) {
        REGISTER_IPC(ecom_server, list_implementations, ecom_list_implementations, "ECom::ListImpls");
        REGISTER_IPC(ecom_server, list_implementations, ecom_list_resolved_implementations, "ECom::ListResolvedImpls");
        REGISTER_IPC(ecom_server, list_implementations, ecom_list_custom_resolved_implementations, "ECom::ListCustomResolvedImpls");
        REGISTER_IPC(ecom_server, get_implementation_creation_method, ecom_get_implementation_creation_method, "ECom::GetImplCreationMethod");
        REGISTER_IPC(ecom_server, collect_implementation_list, ecom_collect_implementations_list, "ECom::CollectImplsList");
    }
}
