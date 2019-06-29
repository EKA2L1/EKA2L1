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

namespace eka2l1::epoc {
    open_font_session_cache_list::open_font_session_cache_list(const int cache_entry_count) {
        cache_offset_array.resize(cache_entry_count);
        session_handle_array.resize(cache_entry_count);
    }
}

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
        fbsfont *font = serv->search_for_cache(spec, size_info.x);

        if (!font) {
            // We need to create new one!
            open_font_info *ofi_suit = serv->seek_the_open_font(spec);

            if (!ofi_suit) {
                ctx->set_request_status(KErrNotFound);
            }

            // Scale it
            epoc::open_font *of = serv->allocate_general_data<epoc::open_font>();
            
            epoc::bitmapfont *bmpfont = server<fbs_server>()->allocate_general_data<epoc::bitmapfont>();
            bmpfont->openfont = serv->host_ptr_to_guest_general_data(of).cast<void>();
            bmpfont->vtable = serv->bmp_font_vtab;

            font = make_new<fbsfont>();
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

            serv->font_cache.push_back(font);
        }

        font_info result_info;
        
        result_info.handle = obj_table_.add(font);
        result_info.address_offset = font->guest_font_handle.ptr_address() - serv->shared_chunk->base().ptr_address();
        result_info.server_handle = static_cast<std::int32_t>(font->id);

        ctx->write_arg_pkg(1, result_info);
        ctx->set_request_status(KErrNone);
    }

    void fbscli::duplicate_font(service::ipc_context *ctx) {
        fbs_server *serv = server<fbs_server>();
        fbsfont *font = serv->search_for_cache_by_id(static_cast<std::uint32_t>(*ctx->get_arg<int>(0)));

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
        fbsfont *font = nullptr;

        auto &cache = server<fbs_server>()->font_cache;
        std::uint32_t addr = static_cast<std::uint32_t>(*ctx->get_arg<int>(0));

        for (std::size_t i = 0; i < cache.size(); i++) {
            if (cache[i]->guest_font_handle.ptr_address() == addr) {
                font = cache[i];
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

    open_font_info *fbs_server::seek_the_open_font(epoc::font_spec &spec) {
        open_font_info *best = nullptr;
        int best_score = -99999999;

        const std::u16string my_name = spec.tf.name.to_std_string(nullptr);

        for (auto &[fam_name, collection]: open_font_store) {
            bool maybe_same_family = (my_name.find(fam_name) != std::string::npos);

            // Seek the way out! Seek the one that contains my name first!
            for (auto &info: collection) {
                int score = 0;

                // Better returns me!
                if (info.face_attrib.name.to_std_string(nullptr) == my_name) {
                    return &info;
                }

                if (maybe_same_family) {
                    score += 100;
                }

                if ((spec.style.flags & epoc::font_style::italic) == (info.face_attrib.style & epoc::open_font_face_attrib::italic)) {
                    score += 50;
                }
                
                if ((spec.style.flags & epoc::font_style::bold) == (info.face_attrib.style & epoc::open_font_face_attrib::bold)) {
                    score += 50;
                }

                if (score > best_score) {
                    best = &info;
                    best_score = score;
                }
            }
        }

        return best;
    }

    fbsfont *fbs_server::search_for_cache(epoc::font_spec &spec, const std::uint32_t desired_max_height) {
        for (std::size_t i = 0; i < font_cache.size(); i++) {
            if (font_cache[i]->of_info.face_attrib.name.to_std_string(nullptr) == spec.tf.name.to_std_string(nullptr)) {
                // Do further test
                if ((desired_max_height != 0 && desired_max_height == font_cache[i]->of_info.metrics.max_height) ||
                    (font_cache[i]->of_info.metrics.design_height == spec.height)) {
                    return font_cache[i];
                }
            }
        }

        return nullptr;
    }

    fbsfont *fbs_server::search_for_cache_by_id(const std::uint32_t id) {    
        auto result = std::lower_bound(font_cache.begin(), font_cache.end(),
            static_cast<std::uint32_t>(id), [](const fbsfont *lhs, const std::uint32_t rhs) {
                return lhs->id < rhs;
            });

        if (result == font_cache.end()) {
            return nullptr;
        }

        return *result;
    }

    void fbsfont::deref() {
        if (count == 1) {
            auto result = std::lower_bound(serv->font_cache.begin(), serv->font_cache.end(),
            static_cast<std::uint32_t>(id), [](const fbsfont *lhs, const std::uint32_t rhs) {
                return lhs->id < rhs;
            });

            if (result != serv->font_cache.end()) {
                serv->font_cache.erase(result);
            }
        }

        epoc::ref_count_object::deref();
    }

    void fbs_server::add_fonts_from_adapter(epoc::adapter::font_file_adapter_instance &adapter) {
        // Iterates through all fonts in this file
        for (std::size_t i = 0; i < adapter->count(); i++) {
            epoc::open_font_face_attrib attrib;
            
            if (!adapter->get_face_attrib(i, attrib)) {
                continue;
            }

            const std::u16string fam_name = attrib.fam_name.to_std_string(nullptr);
            const std::u16string name = attrib.name.to_std_string(nullptr);
            auto &font_fam_collection = open_font_store[fam_name];

            bool found = false;

            // Are we facing duplicate fonts ?
            for (std::size_t i = 0; i < font_fam_collection.size(); i++) {
                if (font_fam_collection[i].face_attrib.name.to_std_string(nullptr) == name) {
                    found = true;
                    break;
                }
            }
            
            // No duplicate font
            if (!found) {
                // Get the metrics and make new open font
                open_font_info info;
                adapter->get_metrics(i, info.metrics);

                info.idx = static_cast<std::int32_t>(i);
                info.face_attrib = attrib;
                info.adapter = adapter.get();

                font_fam_collection.push_back(std::move(info));
            }
        }
            
        font_adapters.push_back(std::move(adapter));
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

                        // Create font file adapter instance
                        auto adapter = epoc::adapter::make_font_file_adapter(epoc::adapter::font_file_adapter_kind::stb,
                            buf);

                        add_fonts_from_adapter(adapter);
                    }
                }

                // TODO: Implement FS callback
            }
        }
    }
}
