/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <core/services/fontbitmap/font.h>
#include <core/services/fontbitmap/fontbitmap.h>
#include <core/services/fontbitmap/op.h>

#include <common/log.h>
#include <core/core.h>

#include <common/e32inc.h>
#include <e32err.h>

#include <core/vfs.h>

#include <filesystem>

namespace fs = std::experimental::filesystem;

namespace eka2l1 {
    fontbitmap_server::fontbitmap_server(system *sys)
        : service::server(sys, "!Fontbitmapserver") {
        REGISTER_IPC(fontbitmap_server, init, EFbsMessInit,
            "FbsServer::Init");
        REGISTER_IPC(fontbitmap_server, get_nearest_font,
            EFbsMessGetNearestFontToMaxHeightInTwips, "Fbs::GetNearestFontToMaxHeightInTwips");
        REGISTER_IPC(fontbitmap_server, get_nearest_font,
            EFbsMessGetNearestFontToDesignHeightInTwips, "Fbs::GetNearestFontToDesignHeightInTwips");
        REGISTER_IPC(fontbitmap_server, get_nearest_font,
            EFbsMessGetNearestFontToDesignHeightInPixels, "Fbs::GetNearestFontToDesignHeightInPixels");
    }

    void fontbitmap_server::init(service::ipc_context ctx) {
        fbs_shared_chunk = ctx.sys->get_kernel_system()->create_chunk(
            "FbsSharedChunk", 0, 0x10000, 0x10000,
            prot::read_write, kernel::chunk_type::disconnected, kernel::chunk_access::global,
            kernel::chunk_attrib::none, kernel::owner_type::process);

        fbs_heap = fast_heap(ctx.sys->get_memory_system(), fbs_shared_chunk, 4);

        if (FT_Init_FreeType(&ft_lib)) {
            LOG_ERROR("Freetype can't not be intialized in font bitmap server!");
            ctx.set_request_status(KErrGeneral);

            return;
        }

        ctx.set_request_status(ctx.sys->get_kernel_system()->open_handle(
            ctx.sys->get_kernel_system()->get_server_by_name("!Fontbitmapserver"), kernel::owner_type::kernel));

        LOG_INFO("FontBitmapServer::Init stubbed (maybe)");
    }

    font *fontbitmap_server::get_cache_font(const std::string &font_path) {
        auto &res = std::find_if(ft_fonts.begin(), ft_fonts.end(),
            [&](const auto &f) { return f.font_path == font_path; });

        if (res != ft_fonts.end()) {
            return &(*res);
        }

        return nullptr;
    }

    font *fontbitmap_server::get_cache_font_by_family_and_style(const std::string &font_fam, uint32_t style) {
        auto &res = std::find_if(ft_fonts.begin(), ft_fonts.end(),
            [&](const auto &f) {
                return (f.font_name == font_fam) && (f.attrib == style);
            });

        if (res != ft_fonts.end()) {
            return &(*res);
        }

        return nullptr;
    }

    void fontbitmap_server::add_font_to_cache(font &f) {
        // Pop
        if (ft_fonts.size() == max_font_cache) {
            font f = std::move(ft_fonts.back());
            FT_Done_Face(f.ft_face);

            ft_fonts.pop();
        }

        ft_fonts.push(std::move(f));
    }

    void fontbitmap_server::do_cache_fonts(io_system *io) {
        std::string real_path = io->get("Z:\\Resource\\Fonts\\");

        for (auto &entry : fs::directory_iterator(real_path)) {
            if (ft_fonts.size() == max_font_cache) {
                break;
            }

            // If this is a file.
            if (entry.status().type() == fs::file_type::regular) {
                // Open the file as TTF font
                FT_Face temp;
                std::string entry_path = entry.path().generic_u8string();

                // Cache fonts
                if (!get_cache_font(entry_path) && !FT_New_Face(ft_lib, entry_path.c_str(), 0, &temp)) {
                    // Search cache if the font is already loaded
                    font tf_font;
                    tf_font.font_name = temp->family_name;
                    tf_font.attrib = temp->style_flags;
                    tf_font.font_path = entry.path().u8string();
                    tf_font.ft_face = temp;

                    add_font_to_cache(tf_font);
                }
            }
        }
    }

    font *fontbitmap_server::get_font(io_system *io, const std::string &font_name, uint32_t style) {
        font *cache = get_cache_font_by_family_and_style(font_name, style);

        if (!cache) {
            std::string real_path = io->get("Z:\\Resource\\Fonts\\");

            // Search for font family and style
            for (auto &entry : fs::directory_iterator(real_path)) {
                // If this is a file.
                if (entry.status().type() == fs::file_type::regular) {
                    // Open the file as TTF font
                    FT_Face temp;
                    std::string entry_path = entry.path().generic_u8string();

                    // Cache fonts
                    if (!get_cache_font(entry_path) && !FT_New_Face(ft_lib, entry_path.c_str(), 0, &temp)
                        && temp->family_name == font_name && temp->style_flags == style) {
                        // Search cache if the font is already loaded
                        font tf_font;
                        tf_font.font_name = temp->family_name;
                        tf_font.attrib = temp->style_flags;
                        tf_font.font_path = entry.path().u8string();
                        tf_font.ft_face = temp;

                        add_font_to_cache(tf_font);

                        return get_cache_font_by_family_and_style(font_name, style);
                    }
                }
            }
        }

        return cache;
    }

    void fontbitmap_server::get_nearest_font(service::ipc_context ctx) {
        // Load font cache if it hasn't been loaded yet.
        if (!cache_loaded) {
            do_cache_fonts(ctx.sys->get_io_system());
            cache_loaded = true;
        }

        auto spec = ctx.get_arg_packed<epoc::fbs::font_spec>(0);
        auto info = ctx.get_arg_packed<epoc::fbs::font_info>(1);
        auto size_info = ctx.get_arg_packed<epoc::fbs::font_size_info>(2);

        if (!spec || !info || !size_info) {
            ctx.set_request_status(KErrArgument);
        }

        // Typeface name.
        std::string typeface_name(spec->tf.name, spec->tf.name + spec->tf.name_length);

        font *requested_font = get_font(ctx.sys->get_io_system(), typeface_name,
            static_cast<uint32_t>(spec->style.flags) & 0x20000 ? spec->style.flags & ~0x20000 : spec->style.flags);

        bool twips = (ctx.msg->function == EFbsMessGetNearestFontToMaxHeightInTwips || ctx.msg->function == EFbsMessGetNearestFontToDesignHeightInTwips)
            ? true
            : false;

        // Allocate resources
        eka2l1::ptr<epoc::fbs::bitmap_font> font_wrapper_vp = fbs_heap.allocate(sizeof(epoc::fbs::bitmap_font))
                                                                  .cast<epoc::fbs::bitmap_font>();

        eka2l1::ptr<epoc::fbs::open_font> open_font_vp = fbs_heap.allocate(sizeof(epoc::fbs::open_font))
                                                             .cast<epoc::fbs::open_font>();

        memory_system *mem = ctx.sys->get_memory_system();

        epoc::fbs::bitmap_font *font_wrapper = font_wrapper_vp.get(mem);
        epoc::fbs::open_font *open_font = open_font_vp.get(mem);

        info->font_handle = 1;
        info->server_handle = unique_id();
        info->font_offset = font_wrapper_vp.ptr_address() - fbs_shared_chunk->base().ptr_address();

        LOG_TRACE("Font address: 0x{:x}", font_wrapper_vp.ptr_address());

        font_wrapper->vtable = ctx.sys->get_lib_manager()->get_vtable_address("CBitmapFont");
        font_wrapper->font_bitmap_off = 0;
        font_wrapper->open_font = open_font_vp.ptr_address();
        font_wrapper->id = info->font_handle;
        font_wrapper->heap = fbs_heap.rheap().ptr_address();

        // Filling alg style
        epoc::fbs::alg_style &style = font_wrapper->style;

        // Filling metrics
        epoc::fbs::open_font_metrics &metrics = open_font->metrics;
        metrics.max_width = requested_font->ft_face->max_advance_width;
        metrics.max_height = requested_font->ft_face->max_advance_height;
        metrics.max_depth = 0;
        metrics.ascent = requested_font->ft_face->ascender;
        metrics.descent = requested_font->ft_face->descender;
        metrics.design_height = spec->height;  // Temporary.

        open_font->glyph_cache_offset = 0x90000000;
        open_font->file_offset = 0;

        ctx.write_arg_pkg(1, *info);
        ctx.set_request_status(KErrNone);
    }

    void fontbitmap_server::destroy() {
        server::destroy();

        FT_Done_FreeType(ft_lib);
    }
}