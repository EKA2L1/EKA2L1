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

#include <epoc/epoc.h>
#include <epoc/vfs.h>

#include <e32err.h>

namespace eka2l1 {
    static bool is_opcode_ruler_twips(const int opcode) {
        return (opcode == fbs_nearest_font_design_height_in_twips || opcode == fbs_nearest_font_max_height_in_twips);
    }

    struct font_info {
        std::int32_t handle;
        std::int32_t address_offset;
        std::int32_t server_handle;
    };

    void fbscli::get_nearest_font(service::ipc_context *ctx) {
        epoc::font_spec spec = *ctx->get_arg_packed<epoc::font_spec>(0);

        // 1 x int of Max height - 2 x int of device size
        eka2l1::vec3 size_info = *ctx->get_arg_packed<eka2l1::vec3>(2);

        const bool is_twips = is_opcode_ruler_twips(ctx->msg->function);

        if (is_twips) {
            // TODO: More proper things
            spec.height = static_cast<std::int32_t>(static_cast<float>(spec.height) / 15);
        }

        fbs_server *serv = server<fbs_server>();

        if (spec.tf.name.get_length() == 0) {
            spec.tf.name = serv->default_system_font;
        }

        // Search the cache
        fbsfont *font = nullptr;
        epoc::open_font_info *ofi_suit = serv->persistent_font_store.seek_the_open_font(spec);

        if (!ofi_suit) {
            ctx->set_request_status(KErrNotFound);
        }

        // Scale it
        epoc::open_font *of = serv->allocate_general_data<epoc::open_font>();
        
        epoc::bitmapfont *bmpfont = server<fbs_server>()->allocate_general_data<epoc::bitmapfont>();
        bmpfont->openfont = serv->host_ptr_to_guest_general_data(of).cast<void>();
        bmpfont->vtable = serv->bmp_font_vtab;

        font = serv->font_obj_container.make_new<fbsfont>();
        font->guest_font_handle = serv->host_ptr_to_guest_general_data(bmpfont).cast<epoc::bitmapfont>();
        font->of_info = *ofi_suit;

        // TODO: Adjust with physical size.            
        float scale_factor = 0;

        if (size_info.x != 0) {
            // Scale for max height
            scale_factor = static_cast<float>(size_info.x) / font->of_info.metrics.max_height;
        } else {
            scale_factor = static_cast<float>(spec.height) / font->of_info.metrics.design_height;
        }

        font->of_info.metrics.max_height = static_cast<std::int16_t>(font->of_info.metrics.max_height * scale_factor);
        font->of_info.metrics.ascent = static_cast<std::int16_t>(font->of_info.metrics.ascent * scale_factor);
        font->of_info.metrics.descent = static_cast<std::int16_t>(font->of_info.metrics.descent * scale_factor);
        font->of_info.metrics.design_height = static_cast<std::int16_t>(font->of_info.metrics.design_height * scale_factor);
        font->of_info.metrics.max_depth = static_cast<std::int16_t>(font->of_info.metrics.max_depth * scale_factor);
        font->of_info.metrics.max_width = static_cast<std::int16_t>(font->of_info.metrics.max_width * scale_factor);
        font->serv = serv;

        of->metrics = font->of_info.metrics;
        of->face_index_offset = static_cast<int>(ofi_suit->idx);
        of->vtable = epoc::DEAD_VTABLE;

        // NOTE: Newer version (from S^3 onwards) uses offset. Older version just cast this directly to integer
        // Since I don't know the version that starts using offset yet, we just leave it be this for now
        of->session_cache_list_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_general_data(
            serv->session_cache_list).ptr_address());

        // TODO (pent0): Fill basic font info to epoc::open_font

        font_info result_info;
        
        result_info.handle = obj_table_.add(font);
        result_info.address_offset = font->guest_font_handle.ptr_address() - serv->shared_chunk->base().ptr_address();
        result_info.server_handle = static_cast<std::int32_t>(font->id);

        ctx->write_arg_pkg(1, result_info);
        ctx->set_request_status(KErrNone);
    }

    void fbscli::duplicate_font(service::ipc_context *ctx) {
        fbs_server *serv = server<fbs_server>();
        fbsfont *font = serv->font_obj_container.get<fbsfont>(*ctx->get_arg<epoc::handle>(0));

        if (!font) {
            ctx->set_request_status(KErrNotFound);
            return;
        }

        font_info result_info;

        // Add new one
        result_info.handle = obj_table_.add(font);
        result_info.address_offset = font->guest_font_handle.ptr_address() - serv->shared_chunk->base().ptr_address();
        result_info.server_handle = static_cast<std::int32_t>(font->id);

        ctx->write_arg_pkg(1, result_info);
        ctx->set_request_status(KErrNone);
    }

    void fbscli::get_face_attrib(service::ipc_context *ctx) {
        // Look for bitmap font with this same handle
        const fbsfont *font = nullptr;
        std::uint32_t addr = static_cast<std::uint32_t>(*ctx->get_arg<int>(0));

        for (const auto &font_cache_obj_ptr: server<fbs_server>()->font_obj_container) {
            const fbsfont *temp_font_ptr = reinterpret_cast<const fbsfont*>(font_cache_obj_ptr.get());

            if (temp_font_ptr && temp_font_ptr->guest_font_handle.ptr_address() == addr) {
                font = temp_font_ptr;
                break;
            }
        }

        if (!font) {
            ctx->set_request_status(false);
            return;
        }
 
        ctx->write_arg_pkg(1, font->of_info.face_attrib);
        ctx->set_request_status(true);
    }

    void fbsfont::deref() {
        epoc::ref_count_object::deref();
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

                        std::vector<std::uint8_t> buf;

                        buf.resize(fsize);
                        f->read_file(&buf[0], 1, static_cast<std::uint32_t>(buf.size()));

                        f->close();

                        // Add fonts
                        persistent_font_store.add_fonts(buf);
                    }
                }

                // TODO: Implement FS callback
            }
        }
    }
}
