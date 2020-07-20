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
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>

#include <services/ecom/ecom.h>
#include <vfs/vfs.h>

#include <epoc/epoc.h>
#include <loader/spi.h>
#include <common/uid.h>

#include <common/wildcard.h>
#include <services/ecom/common.h>

#include <utils/bafl.h>
#include <utils/err.h>

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
        auto ecom_private_dir = io->open_dir(pattern, io_attrib::include_file);

        if (!ecom_private_dir) {
            // Again folder not found or error from our side
            LOG_ERROR("Private directory of ecom can't be accessed");
            return {};
        }

        std::vector<std::string> results;

        while (auto entry = ecom_private_dir->get_next_entry()) {
            if (utils::is_file_compatible_with_language(entry->full_path, ".spi", sys->get_system_language())) {
                results.push_back(entry->full_path);
            }
        }

        return results;
    }

    bool ecom_server::load_and_install_plugin_from_buffer(const std::u16string &name, std::uint8_t *buf, const std::size_t size,
        const drive_number drv) {
        common::ro_buf_stream stream(buf, size);
        loader::rsc_file rsc(reinterpret_cast<common::ro_stream *>(&stream));

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

    bool ecom_server::get_resolved_implementations(std::vector<ecom_implementation_info_ptr> &collect_vector, const epoc::uid interface_uid, const ecom_resolver_params &params, const bool generic_wildcard_match) {
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
            return false;
        }

        // An implementation is considered to be accepted through these conditions:
        // - All extended interfaces given are available in the implementation
        // - Match the wildcard (if wildcard not empty)

        // May want to construct a regex for name comparing if we use generic match
        std::regex wildcard_matcher;

        if (generic_wildcard_match && !params.match_string_.empty()) {
            wildcard_matcher = std::move(std::regex(common::wildcard_to_regex_string(params.match_string_)));
        }

        // Iterate through all implementations
        for (ecom_implementation_info_ptr &implementation : interface_ite->second.implementations) {
            // Check the extended interfaces first
            bool satisfy = true;

            for (epoc::uid given_extended_interface : params.extended_interfaces_) {
                if (std::lower_bound(implementation->extended_interfaces.begin(), implementation->extended_interfaces.end(),
                        given_extended_interface)
                    == implementation->extended_interfaces.end()) {
                    satisfy = false;
                    break;
                }
            }

            // Not satisfy ? Yeah, let's just turn back
            if (!satisfy) {
                continue;
            }

            // Match string empty, so we should add into nice stuff now
            if (params.match_string_.empty()) {
                collect_vector.push_back(implementation);
                continue;
            }

            satisfy = false;

            // We still need to see if the name is match
            // Generic match ? Wildcard check
            if (generic_wildcard_match) {
                if (std::regex_match(implementation->default_data, wildcard_matcher)) {
                    satisfy = true;
                }
            } else {
                if (implementation->default_data.find(params.match_string_) != std::string::npos) {
                    satisfy = true;
                }
            }

            // TODO(pent0): Capability supply

            if (satisfy) {
                collect_vector.push_back(implementation);
            }
        }

        return true;
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

        auto plugin_dir = io->open_dir(plugin_dir_path, io_attrib::include_file);
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

        create_session<ecom_session>(&ctx);
        ctx.complete(epoc::error_none);
    }

    ecom_session::ecom_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version client_ver)
        : typical_session(svr, client_ss_uid, client_ver) {
    }

    void ecom_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case ecom_list_implementations:
        case ecom_list_resolved_implementations:
        case ecom_list_custom_resolved_implementations:
            list_implementations(ctx);
            break;

        case ecom_get_implementation_creation_method:
        case ecom_get_resolved_creation_method:
        case ecom_get_custom_resolved_creation_method:
            get_implementation_creation_method(ctx);
            break;

        case ecom_collect_implementations_list:
            collect_implementation_list(ctx);
            break;

        default:
            LOG_ERROR("Unimplemented ecom session opcode {}", ctx->msg->function);
            break;
        }
    }

    bool ecom_session::get_implementation_buffer(std::uint8_t *buf, const std::size_t buf_size,
        const bool support_extended_interface) {
        if (buf == nullptr) {
            return false;
        }

        common::chunkyseri seri(buf, buf_size, common::SERI_MODE_WRITE);

        std::uint32_t total_impls = static_cast<std::uint32_t>(collected_impls_.size());
        seri.absorb(total_impls);

        for (auto &impl : collected_impls_) {
            if (seri.eos()) {
                return false;
            }

            impl->do_state(seri, support_extended_interface, is_using_old_ecom_abi());
        }

        return true;
    }

    bool ecom_session::unpack_match_str_and_extended_interfaces(std::string &data, std::string &match_str,
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

    bool ecom_session::get_resolver_params(service::ipc_context *ctx, const int index, ecom_resolver_params &params) {
        const bool support_extended_interface = ctx->sys->get_symbian_version_use() > epocver::epoc94;

        // I only want to do reading in a scope since these will be useless really soon
        {
            // The second IPC argument contains the name match string and extend interface list
            // Let's do some reading
            std::string arg2_data;

            if (auto arg2_data_op = ctx->get_argument_value<std::string>(1)) {
                arg2_data = std::move(arg2_data_op.value());
            } else {
                ctx->complete(epoc::error_argument);
                return false;
            }

            if (!support_extended_interface) {
                params.match_string_ = arg2_data;
            } else {
                if (!unpack_match_str_and_extended_interfaces(arg2_data, params.match_string_, params.extended_interfaces_)) {
                    // TODO: This is some mysterious here.
                    ctx->complete(epoc::error_argument);
                    return false;
                }
            }
        }

        return true;
    }
    
    void ecom_session::list_implementations(service::ipc_context *ctx) {
        // Clear last cache
        collected_impls_.clear();

        // TODO: See if there is any exception (eg N97 could support it, not sure).
        const bool support_extended_interface = ctx->sys->get_symbian_version_use() > epocver::epoc94;

        // The UID type is the first parameter
        // First UID contains the interface UID, while the third one contains resolver UID
        // The middle UID is reserved
        epoc::uid_type uids;

        // Use chunkyseri
        if (auto uids_result = ctx->get_argument_data_from_descriptor<epoc::uid_type>(0)) {
            uids = std::move(uids_result.value());
        } else {
            ctx->complete(epoc::error_argument);
            return;
        }

        ecom_resolver_params resolve_params;
        if (!get_resolver_params(ctx, 1, resolve_params)) {
            // The error is already set in that function
            return;
        }

        // Get list implementations extra parameters
        ecom_list_impl_param list_impl_param;
        bool list_param_available = false;

        if (auto list_impl_param_op = ctx->get_argument_data_from_descriptor<ecom_list_impl_param>(2)) {
            list_impl_param = std::move(list_impl_param_op.value());
            list_param_available = true;
        } else {
            // TODO(pent0): Really...?
            list_impl_param.match_type = true;
        }

        set_using_old_ecom_abi(!list_param_available);

        // Open a serializer for measure data size
        common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
        std::uint32_t total_impls = 0;

        // Absorb padding
        seri.absorb(total_impls);

        if (!server<ecom_server>()->get_resolved_implementations(collected_impls_, uids[epoc::ecom_interface_uid_index], resolve_params, list_impl_param.match_type)) {
            ctx->complete(epoc::ecom_no_interface_identified);
            return;
        }

        if (collected_impls_.empty()) {
            ctx->complete(epoc::ecom_no_registration_identified);
            return;
        }

        for (ecom_implementation_info_ptr &implementation: collected_impls_) {
            implementation->do_state(seri, support_extended_interface, is_using_old_ecom_abi());
        }

        // Compare the total buffer size we are going to write with the one guest provided
        // If it's not sufficient enough, throw epoc::error_overflow
        const std::size_t total_buffer_size_require = seri.size();
        const std::size_t total_buffer_size_given = list_impl_param.buffer_size;

        list_impl_param.buffer_size = static_cast<const int>(total_buffer_size_require);

        if ((list_param_available) && (total_buffer_size_require > total_buffer_size_given)) {
            // Write new list impl param, which contains the required new size
            ctx->write_data_to_descriptor_argument<ecom_list_impl_param>(2, list_impl_param, nullptr, true);
            ctx->complete(epoc::error_overflow);

            return;
        }

        // Write list implementation param to tell the client how much bytes we actually write
        if (list_param_available) {
            // Write the buffer
            // This must not fail
            if (!get_implementation_buffer(ctx->get_descriptor_argument_ptr(3), total_buffer_size_given, support_extended_interface)) {
                ctx->complete(epoc::error_argument);
                return;
            }

            ctx->write_data_to_descriptor_argument<ecom_list_impl_param>(2, list_impl_param, nullptr, true);

            // Set the buffer length. It's not I like it or anything, baka
            ctx->set_descriptor_argument_length(3, static_cast<const std::uint32_t>(total_buffer_size_require));
        }

        // Finally, returns
        if (!list_param_available) {
            const std::uint32_t size_require_casted = static_cast<std::uint32_t>(total_buffer_size_require);
            ctx->write_data_to_descriptor_argument<std::uint32_t>(3, size_require_casted);
        }

        ctx->complete(epoc::error_none);
    }

    void ecom_session::get_implementation_creation_method(service::ipc_context *ctx) {
        do_get_resolved_impl_creation_method(ctx);
    }

    void ecom_session::collect_implementation_list(service::ipc_context *ctx) {
        const bool support_extended_interface = ctx->sys->get_symbian_version_use() > epocver::epoc94;
        const int slot_to_operate = is_using_old_ecom_abi() ? 3 : 0;
        const std::size_t total_buffer_size_given = ctx->get_argument_max_data_size(slot_to_operate);

        if (!get_implementation_buffer(ctx->get_descriptor_argument_ptr(slot_to_operate), total_buffer_size_given, support_extended_interface)) {
            ctx->complete(epoc::error_overflow);
            return;
        }

        ctx->set_descriptor_argument_length(slot_to_operate, static_cast<std::uint32_t>(total_buffer_size_given));

        collected_impls_.clear();
        ctx->complete(epoc::error_none);
    }

    ecom_server::ecom_server(eka2l1::system *sys)
        : service::typical_server(sys, "!ecomserver") {
    }
}
