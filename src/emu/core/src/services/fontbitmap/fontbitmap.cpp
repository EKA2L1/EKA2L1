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

#include <services/fontbitmap/font.h>
#include <services/fontbitmap/fontbitmap.h>
#include <services/fontbitmap/op.h>

#include <common/log.h>
#include <core.h>

#include <common/e32inc.h>
#include <e32err.h>

#include <vfs.h>

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
            "FbsSharedChunk", 0, 0x5000, 0x5000,
            prot::read_write, kernel::chunk_type::disconnected, kernel::chunk_access::global,
            kernel::chunk_attrib::none, kernel::owner_type::process);

        if (FT_Init_FreeType(&ft_lib)) {
            LOG_ERROR("Freetype can't not be intialized in font bitmap server!");
            ctx.set_request_status(KErrGeneral);

            return;
        }

        ctx.set_request_status(obj_id);
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

    font* fontbitmap_server::get_font(io_system *io, const std::string &font_name, uint32_t style) {
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

        std::string typeface_name(spec->tf.name, spec->tf.name + spec->tf.name_length);
        font *requested_font = get_font(ctx.sys->get_io_system(), typeface_name, 
            static_cast<uint32_t>(spec->style.flags) & 0x20000 ? spec->style.flags & ~0x20000 : spec->style.flags);

        // TODO: Find out if font is loaded as bitmap to memory or not

        ctx.set_request_status(KErrNone);
    }

    void fontbitmap_server::destroy() {
        server::destroy();

        FT_Done_FreeType(ft_lib);
    }
}