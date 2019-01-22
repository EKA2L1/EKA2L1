/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <epoc/services/fbs/fbs.h>
#include <epoc/epoc.h>
#include <epoc/kernel.h>

#include <common/log.h>
#include <common/e32inc.h>
#include <common/cvt.h>
#include <epoc/vfs.h>

#include <e32err.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace eka2l1 {
    std::uint32_t fbshandles::make_handle(std::size_t index) {
        return (owner->session_id << 16) | static_cast<std::uint16_t>(index);
    }

    std::uint32_t fbshandles::add_object(fbsobj *obj) {
        for (std::size_t i = 0 ; i < objects.size() ; i++) {
            if (objects[i] == nullptr) {
                objects[i] = obj;
                return make_handle(i);
            }
        }

        objects.push_back(obj);
        return make_handle(objects.size() - 1);
    }

    bool fbshandles::remove_object(std::size_t index) {
        if (objects.size() >= index) {
            return false;
        }

        objects[index] = nullptr;
        return true;
    }

    fbsobj *fbshandles::get_object(const std::uint32_t handle) {
        const std::uint16_t ss_id = handle >> 16;

        if (ss_id != owner->session_id) {
            LOG_CRITICAL("Fail safe check: FBS handle informs a session id that does not match with current session id");
            return nullptr;
        }

        const std::uint16_t index = static_cast<std::uint16_t>(handle);

        if (objects.size() >= index) {
            return false;
        }

        return objects[index];
    }
    
    fbscli::fbscli(fbs_server *serv, const std::uint32_t ss_id) 
        : server(serv), session_id(ss_id) {
        
    }

    fbs_server::fbs_server(eka2l1::system *sys) 
        : service::server(sys, "!Fontbitmapserver", true) {
        REGISTER_IPC(fbs_server, init, fbs_init, "Fbs::Init");
    }

    fbscli *fbs_server::get_session_associated_with_handle(const std::uint32_t handle) {
        const std::uint32_t ss_id = handle >> 16;
        auto result = clients.find(ss_id);

        if (result == clients.end()) {
            return nullptr;
        }

        return &result->second;
    }

    fbsobj *fbs_server::get_object(const std::uint32_t handle) {
        fbscli *cli = get_session_associated_with_handle(handle);

        if (cli == nullptr) {
            return nullptr;
        }

        return cli->handles.get_object(handle);
    }

    void fbs_server::load_fonts(eka2l1::io_system *io) {
        // Put it out here, so to if the file is smaller than last file, no reallocation is needed.
        std::vector<std::uint8_t> buf;

        // Search all drives
        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
            if (io->get_drive_entry(drv)) {
                const std::u16string fonts_folder_path = std::u16string { drive_to_char16(drv) } + u":\\Resource\\Fonts\\";
                auto folder = io->open_dir(fonts_folder_path, io_attrib::none);
                
                if (folder) {
                    while (auto entry = folder->get_next_entry()) {
                        symfile f = io->open_file(common::utf8_to_ucs2(entry->full_path), READ_MODE | BIN_MODE);
                        const std::uint64_t fsize = f->size();

                        buf.resize(fsize);
                        f->read_file(&buf[0], 1, static_cast<std::uint32_t>(buf.size()));

                        f->close();

                        fbsfont server_font(id_counter++);
                        server_font.guest_font_handle = 0;
                        server_font.stb_handle = std::make_unique<stbtt_fontinfo>();

                        if (stbtt_InitFont(server_font.stb_handle.get(), buf.data(), 0) != 0) {
                            // We success, let's continue! We can't give up...
                            // 決定! それは私のものです
                            
                            // Movingg....
                            // We are not going to extract the font name now, since it's complicated.
                            font_avails.push_back(std::move(server_font));
                        }
                    }
                }

                // TODO: Implement FS callback
            }
        }
    }

    void fbs_server::init(service::ipc_context context) {
        if (!shared_chunk && !large_chunk) {
            // Initialize those chunks
            kernel_system *kern = context.sys->get_kernel_system();
            std::uint32_t shared_chunk_handle = kern->create_chunk(
                "FbsSharedChunk",
                0,
                0x10000,
                0x200000,
                prot::read_write,
                kernel::chunk_type::disconnected,
                kernel::chunk_access::global,
                kernel::chunk_attrib::none,
                kernel::owner_type::kernel
            );

            std::uint32_t large_chunk_handle = kern->create_chunk(
                "FbsLargeChunk",
                0,
                0,
                0x2000000,
                prot::read_write,
                kernel::chunk_type::disconnected,
                kernel::chunk_access::global,
                kernel::chunk_attrib::none,
                kernel::owner_type::kernel
            );

            if (shared_chunk_handle == INVALID_HANDLE || large_chunk_handle == INVALID_HANDLE) {
                LOG_CRITICAL("Can't create shared chunk and large chunk of FBS, exiting");
                context.set_request_status(KErrNoMemory);

                return;
            }

            shared_chunk = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(shared_chunk_handle));
            large_chunk = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(large_chunk_handle));

            // Probably also indicates that font aren't loaded yet
            load_fonts(context.sys->get_io_system());
        }

        // Create new server client
        const std::uint32_t ss_id = context.msg->msg_session->unique_id();
        fbscli cli(this, ss_id); 

        clients.emplace(ss_id, std::move(cli));
        context.set_request_status(ss_id);
    }
}