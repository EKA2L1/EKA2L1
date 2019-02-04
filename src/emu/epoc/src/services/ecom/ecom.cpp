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

#include <common/buffer.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>

#include <epoc/services/ecom/ecom.h>
#include <epoc/vfs.h>

#include <epoc/loader/spi.h>
#include <epoc/epoc.h>

namespace eka2l1 {
    bool ecom_server::register_implementation(const std::uint32_t interface_uid,
        ecom_implementation_info &impl) {
        auto &interface = interfaces[interface_uid];
        auto impl_ite = std::find_if(interface.implementations.begin(), interface.implementations.end(),
            [&](const ecom_implementation_info &impl2) { return impl.uid == impl.uid; });

        if (impl_ite == interface.implementations.end()) {
            interface.implementations.push_back(std::move(impl));
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

    bool ecom_server::load_and_install_plugins_from_buffer(std::uint8_t *buf, const std::size_t size) {
        common::ro_buf_stream stream(buf, size);
        loader::rsc_file rsc(stream);

        ecom_plugin plugin;
        bool result = load_plugin(rsc, plugin);

        if (!result) {
            return false;
        }

        for (auto &pinterface: plugin.interfaces) {
            // Get from the current interface on server
            auto &interface_on_server = interfaces[pinterface.uid];
            interface_on_server.uid = pinterface.uid;
            
            for (auto &impl: pinterface.implementations) {
                if (!register_implementation(pinterface.uid, impl)) {
                    return false;
                }
            }
    
            pinterface.implementations.clear();
        }

        return true;
    }

    bool ecom_server::load_archives(eka2l1::io_system *io) {
        std::vector<std::string> archives = get_ecom_plugin_archives(io);

        for (const std::string &archive: archives) {
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

            for (auto &entry: spi.entries) {
                result = load_and_install_plugins_from_buffer(&entry.file[0], entry.file.size());
                
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

            if (!load_and_install_plugins_from_buffer(&dat[0], dat.size())) {
                LOG_ERROR("Can't load and install plugins description {}", entry->name);
                return false;
            }
        }

        // TODO: Register a notification to the server side

        return true;
    }

    void ecom_server::list_impls(service::ipc_context ctx) {
        // TODO
    }

    void ecom_server::connect(service::ipc_context ctx) {
        if (!init) {
            if (!load_plugins(ctx.sys->get_io_system())) {
                LOG_ERROR("An error happens with initialization of ECom");
            }
        }

        init = true;
    }
    
    ecom_server::ecom_server(eka2l1::system *sys)
        : service::server(sys, "!ecomserver", true) {
        REGISTER_IPC(ecom_server, list_impls, ecom_list_implementations, "ECom::ListImpls");
        REGISTER_IPC(ecom_server, list_impls, ecom_list_resolved_implementations, "ECom::ListResolvedImpls");
    }

}