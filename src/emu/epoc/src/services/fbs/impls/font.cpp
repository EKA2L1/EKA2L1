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

#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/log.h>
#include <common/vecx.h>

#include <epoc/vfs.h>

#include <e32err.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace eka2l1 {
    static bool is_opcode_ruler_twips(const int opcode) {
        return (opcode == fbs_nearest_font_design_height_in_twips || opcode == fbs_nearest_font_max_height_in_twips);
    }

    void fbscli::get_nearest_font(service::ipc_context *ctx) {
        epoc::font_spec spec = *ctx->get_arg_packed<epoc::font_spec>(0);
        eka2l1::vec2 size_info = *ctx->get_arg_packed<eka2l1::vec2>(2);

        const bool is_twips = is_opcode_ruler_twips(ctx->msg->function);

        // Search for the name first
        const std::string font_name = common::ucs2_to_utf8(spec.tf.name.to_std_string(ctx->msg->own_thr->owning_process()));

        fbsfont *match = nullptr;

        for (auto &font : server<fbs_server>()->font_avails) {
            if (stbtt_FindMatchingFont(font->data.data(), font_name.data(), 0) != -1) {
                // Hey, you are the choosen one
                match = font;
                break;
            }
        }

        if (!match) {
            // TODO: We are going to see other fonts that should match the description
        }

        if (!match) {
            ctx->set_request_status(KErrNotFound);
            return;
        }

        if (!match->guest_font_handle) {
            // Initialize them all while we don't need it is wasting emulator time and resources
            // So, when it need one, we are gonna create font info
            epoc::bitmapfont *bmpfont = server<fbs_server>()->allocate_general_data<epoc::bitmapfont>();
            match->guest_font_handle = server<fbs_server>()->host_ptr_to_guest_general_data(bmpfont).cast<epoc::bitmapfont>();

            // TODO: Find out how to get font name easily
        }

        ctx->set_request_status(KErrNone);
    }

    void fbs_server::load_fonts(eka2l1::io_system *io) {
        // Search all drives
        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
            if (io->get_drive_entry(drv)) {
                const std::u16string fonts_folder_path = std::u16string{ drive_to_char16(drv) } + u":\\Resource\\Fonts\\";
                auto folder = io->open_dir(fonts_folder_path, io_attrib::none);

                if (folder) {
                    LOG_TRACE("Found font folder: {}", common::ucs2_to_utf8(fonts_folder_path));

                    while (auto entry = folder->get_next_entry()) {
                        symfile f = io->open_file(common::utf8_to_ucs2(entry->full_path), READ_MODE | BIN_MODE);
                        const std::uint64_t fsize = f->size();

                        // Put it out here, so to if the file is smaller than last file, no reallocation is needed.
                        std::vector<std::uint8_t> buf;

                        buf.resize(fsize);
                        f->read_file(&buf[0], 1, static_cast<std::uint32_t>(buf.size()));

                        f->close();

                        fbsfont *server_font = make_new<fbsfont>();
                        server_font->guest_font_handle = 0;
                        server_font->stb_handle = std::make_unique<stbtt_fontinfo>();

                        if (stbtt_InitFont(server_font->stb_handle.get(), buf.data(), 0) != 0) {
                            // We success, let's continue! We can't give up...
                            // 決定! それは私のものです
                            server_font->data = std::move(buf);

                            // Movingg....
                            // We are not going to extract the font name now, since it's complicated.
                            font_avails.push_back(server_font);
                        }
                    }
                }

                // TODO: Implement FS callback
            }
        }
    }
}
