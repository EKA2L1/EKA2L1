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

#include <services/fbs/fbs.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/vecx.h>

#include <system/epoc.h>
#include <vfs/vfs.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    void open_font_glyph_offset_array::init(fbscli *cli, const std::int32_t count) {
        offset_array_count = count;

        std::int32_t *offset_array_ptr = reinterpret_cast<std::int32_t *>(cli->server<fbs_server>()
                                                                              ->allocate_general_data_impl(count * sizeof(std::int32_t)));

        std::fill(offset_array_ptr, offset_array_ptr + count, 0);

        // Set offset array offset
        if (epoc::does_client_use_pointer_instead_of_offset(cli)) {
            offset_array_offset = static_cast<std::int32_t>(
                cli->server<fbs_server>()->host_ptr_to_guest_general_data(offset_array_ptr).ptr_address());
        } else {
            offset_array_offset = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(offset_array_ptr) - reinterpret_cast<std::uint8_t *>(this));
        }
    }

    std::int32_t *open_font_glyph_offset_array::pointer(fbscli *cli) {
        if (offset_array_offset == 0) {
            return nullptr;
        }

        fbs_server *serv = cli->server<fbs_server>();

        if (epoc::does_client_use_pointer_instead_of_offset(cli)) {
            return eka2l1::ptr<std::int32_t>(offset_array_offset).get(serv->get_system()->get_memory_system());
        }

        return reinterpret_cast<std::int32_t *>(reinterpret_cast<std::uint8_t *>(this) + offset_array_offset);
    }

    void *open_font_glyph_offset_array::get_glyph(fbscli *client, const std::int32_t idx) {
        if (idx < 0 || idx >= offset_array_count) {
            return nullptr;
        }

        // Get offset array pointer
        std::int32_t *offset_array_ptr = pointer(client);

        if (offset_array_ptr[idx] == 0) {
            return nullptr;
        }

        fbs_server *serv = client->server<fbs_server>();

        if (epoc::does_client_use_pointer_instead_of_offset(client)) {
            return eka2l1::ptr<void>(offset_array_ptr[idx]).get(serv->get_system()->get_memory_system());
        }

        return reinterpret_cast<std::uint8_t *>(this) + offset_array_ptr[idx];
    }

    bool open_font_glyph_offset_array::set_glyph(fbscli *client, const std::int32_t idx, void *cache_entry) {
        if (idx < 0 || idx >= offset_array_count) {
            return false;
        }

        // Get offset array pointer
        std::int32_t *offset_array_ptr = pointer(client);

        if (!offset_array_ptr) {
            return false;
        }

        if (epoc::does_client_use_pointer_instead_of_offset(client)) {
            offset_array_ptr[idx] = static_cast<std::int32_t>(
                client->server<fbs_server>()->host_ptr_to_guest_general_data(cache_entry).ptr_address());
        } else {
            offset_array_ptr[idx] = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(cache_entry) - reinterpret_cast<std::uint8_t *>(this));
        }

        return true;
    }

    bool open_font_glyph_offset_array::is_entry_empty(fbscli *cli, const std::int32_t idx) {
        if (idx < 0 || idx >= offset_array_count) {
            return true;
        }

        auto ptr = pointer(cli);
        if (!ptr) {
            return false;
        }

        return (ptr[idx] == 0);
    }

    open_font_session_cache_v3 *open_font_session_cache_list::get(fbscli *cli, const std::int32_t session_handle, const bool create) {
        const std::int32_t *offset_ptr = find(session_handle);
        fbs_server *serv = cli->server<fbs_server>();

        if (!offset_ptr) {
            // Create new one.
            if (create) {
                open_font_session_cache_v3 *cache = serv->allocate_general_data<open_font_session_cache_v3>();
                const std::int32_t offset = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(cache) - reinterpret_cast<std::uint8_t *>(this));

                cache->session_handle = session_handle;
                cache->random_seed = 0;

                // Allocate offset array
                cache->offset_array.init(cli, NORMAL_SESSION_CACHE_ENTRY_COUNT);

                add(session_handle, offset);
                return cache;
            } else {
                return nullptr;
            }
        }

        if (epoc::does_client_use_pointer_instead_of_offset(cli)) {
            return eka2l1::ptr<open_font_session_cache_v3>(*offset_ptr).get(serv->get_system()->get_memory_system());
        }

        return reinterpret_cast<open_font_session_cache_v3 *>(reinterpret_cast<std::uint8_t *>(this) + *offset_ptr);
    }

    open_font_session_cache_link *open_font_session_cache_link::get_or_create(fbscli *cli) {
        open_font_session_cache_link *current = this;
        fbs_server *serv = cli->server<fbs_server>();
        memory_system *mem = serv->get_system()->get_memory_system();

        do {
            if (!current->next) {
                break;
            }

            current = current->next.get(mem);

            auto current_cache = current->cache.get(mem);

            if (current_cache->session_handle == cli->connection_id_) {
                return current;
            }
        } while (true);

        // Doesn't exist in the linked list, create new one
        // Allocate the link
        auto next_link = serv->allocate_general_data<open_font_session_cache_link>();
        current->next = serv->host_ptr_to_guest_general_data(next_link).cast<epoc::open_font_session_cache_link>();

        open_font_session_cache_old *cache = serv->allocate_general_data<open_font_session_cache_old>();

        cache->session_handle = cli->connection_id_;
        cache->offset_array.init(cli, NORMAL_SESSION_CACHE_ENTRY_COUNT);
        cache->last_use_counter = 0;

        // Set the cache guest pointer
        next_link->cache = serv->host_ptr_to_guest_general_data(cache).cast<epoc::open_font_session_cache_old>();

        return next_link;
    }

    bool open_font_session_cache_link::remove(fbscli *cli) {
        open_font_session_cache_link *previous = nullptr;
        open_font_session_cache_link *current = this;
        fbs_server *serv = cli->server<fbs_server>();
        memory_system *mem = serv->get_system()->get_memory_system();

        do {
            if (!current->next) {
                break;
            }

            previous = current;
            current = current->next.get(mem);

            auto current_cache = current->cache.get(mem);

            if (current_cache->session_handle == cli->connection_id_) {
                previous->next = current->next;

                // Free current link
                if (serv->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
                    // EKA1 has a weird thing...
                    current_cache->destroy<epoc::open_font_session_cache_entry_v1>(cli);
                } else {
                    current_cache->destroy<epoc::open_font_session_cache_entry_v2>(cli);
                }

                serv->free_general_data(current_cache);
                serv->free_general_data(current);
                return true;
            }
        } while (true);

        // We search the world but didn't find it. Ahoi! Baby! Why did you do it!! Searching for me while
        // i'm hiding from evil around the world....
        return false;
    }

    bool open_font_session_cache_list::erase_cache(fbscli *cli) {
        auto cache = get(cli, cli->connection_id_, false);

        if (cache) {
            cache->destroy(cli);
            cli->server<fbs_server>()->free_general_data(cache);

            // Remove it from the root
            this->remove(cli->connection_id_);

            return true;
        }

        return false;
    }

    void open_font_glyph_v2::destroy(fbscli *cli) {
        // Bitmap data allocated together with glyph info
    }

    void open_font_glyph_v3::destroy(fbscli *cli) {
        // Bitmap data allocated together with glyph info
    }

    void open_font_session_cache_v3::destroy(fbscli *cli) {
        for (std::int32_t i = 0; i < offset_array.offset_array_count; i++) {
            if (!offset_array.is_entry_empty(cli, i)) {
                auto glyph_cache = offset_array.get_glyph(cli, i);
                auto serv = cli->server<fbs_server>();
                reinterpret_cast<open_font_session_cache_entry_v3 *>(glyph_cache)->destroy(cli);
                serv->free_general_data(glyph_cache);
            }
        }
    }

    void open_font_session_cache_v3::add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph) {
        const std::uint32_t real_index = (code & 0x7fffffff) % offset_array.offset_array_count;

        if (!offset_array.is_entry_empty(cli, real_index)) {
            auto glyph_cache = offset_array.get_glyph(cli, real_index);
            reinterpret_cast<open_font_session_cache_entry_v3 *>(glyph_cache)->destroy(cli);
            cli->server<fbs_server>()->free_general_data(glyph_cache);
        }

        offset_array.set_glyph(cli, real_index, the_glyph);
    }

    template <typename T>
    void open_font_session_cache_old::destroy(fbscli *cli) {
        for (std::int32_t i = 0; i < offset_array.offset_array_count; i++) {
            if (!offset_array.is_entry_empty(cli, i)) {
                auto glyph_cache = offset_array.get_glyph(cli, i);
                auto serv = cli->server<fbs_server>();
                reinterpret_cast<T *>(glyph_cache)->destroy(cli);
                serv->free_general_data(glyph_cache);
            }
        }
    }

    template <typename T>
    void open_font_session_cache_old::add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph) {
        std::uint32_t real_index = (code & 0x7fffffff) % offset_array.offset_array_count;

        if (!offset_array.is_entry_empty(cli, real_index)) {
            // Free at given address.// Search for the least last used
            auto ptr = offset_array.pointer(cli);
            fbs_server *serv = cli->server<fbs_server>();
            memory_system *mem = serv->get_system()->get_memory_system();

            std::int32_t least_use = 99999999;

            for (std::int32_t i = 0; i < offset_array.offset_array_count; i++) {
                if (!ptr[i]) {
                    real_index = i;
                    break;
                }

                auto entry = eka2l1::ptr<T>(ptr[i]).get(mem);

                if (entry->last_use < least_use) {
                    least_use = entry->last_use;
                    real_index = i;
                }
            }
        }

        if (!offset_array.is_entry_empty(cli, real_index)) {
            auto glyph_cache = offset_array.get_glyph(cli, real_index);
            reinterpret_cast<T *>(glyph_cache)->destroy(cli);

            cli->server<fbs_server>()->free_general_data(glyph_cache);
        }

        reinterpret_cast<T *>(the_glyph)->glyph_index = real_index;
        offset_array.set_glyph(cli, real_index, the_glyph);
    }

    template void open_font_session_cache_old::add_glyph<open_font_session_cache_entry_v1>(fbscli *cli, const std::uint32_t code, void *the_glyph);
    template void open_font_session_cache_old::add_glyph<open_font_session_cache_entry_v2>(fbscli *cli, const std::uint32_t code, void *the_glyph);

    template void open_font_session_cache_old::destroy<open_font_session_cache_entry_v1>(fbscli *cli);
    template void open_font_session_cache_old::destroy<open_font_session_cache_entry_v2>(fbscli *cli);

    open_font_glyph_cache_v1::open_font_glyph_cache_v1()
        : entry_(0)
        , unk4_(0)
        , unk8_(0) {
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

    fbsfont *fbs_server::look_for_font_with_address(const eka2l1::address addr) {
        const address base_shared_old_mm_model = shared_chunk->base(nullptr).ptr_address();

        for (auto &font_cache_obj_ptr : font_obj_container) {
            fbsfont *temp_font_ptr = reinterpret_cast<fbsfont *>(font_cache_obj_ptr.get());

            if (temp_font_ptr && (temp_font_ptr->guest_font_offset + base_shared_old_mm_model) == addr) {
                return temp_font_ptr;
            }
        }

        return nullptr;
    }

    static void do_scale_metrics(epoc::open_font_metrics &metrics, const float scale_x, const float scale_y) {
        metrics.max_height = static_cast<std::int16_t>(metrics.max_height * scale_y);
        metrics.ascent = static_cast<std::int16_t>(metrics.ascent * scale_y);
        metrics.descent = static_cast<std::int16_t>(metrics.descent * scale_y);
        metrics.design_height = static_cast<std::int16_t>(metrics.design_height * scale_y);
        metrics.max_depth = static_cast<std::int16_t>(metrics.max_depth * scale_y);
        metrics.max_width = static_cast<std::int16_t>(metrics.max_width * scale_x);
    }

    static std::int32_t calculate_baseline(epoc::font_spec_base &spec) {
        if (static_cast<epoc::font_spec_v1 &>(spec).style.flags & epoc::font_style_base::super) {
            // Superscript, see 2^5 for example, 5 is the superscript
            constexpr std::int32_t super_script_offset_percentage = -28;
            return super_script_offset_percentage * spec.height / 100;
        }

        if (static_cast<epoc::font_spec_v1 &>(spec).style.flags & epoc::font_style_base::sub) {
            // Subscript, it's the opposite with superscript. HNO3 for example, in chemistry,
            // 3 supposed to be below HNO
            constexpr std::int32_t subscript_offset_percentage = 14;
            return subscript_offset_percentage * spec.height / 100;
        }

        // Nothing special... Move along.
        return 0;
    }

    static void calculate_algorithic_style(epoc::alg_style &style, epoc::font_spec_base &spec) {
        style.baseline_offsets_in_pixel = calculate_baseline(spec);

        style.height_factor = 1;
        style.width_factor = 1;

        // TODO: Make font adapter reports supported style back.
        // For now, you! Yes! Stay 0.
        style.flags = 0;
    }

    /**
     * \brief Fill bitmap font's spec in twips with given client spec and adjusted height.
     * \param target_spec         The bitmap font's spec to fill.
     * \param given_spec          The spec given by the client from IPC, in pixels.
     * \param adjusted_height     The adjusted height in pixels.
     * \param adapter             The adapter for the target bitmap font that we will fill the spec.
     */
    static void do_fill_bitmap_font_spec(epoc::font_spec_base &target_spec, epoc::font_spec_base &given_spec,
        std::int16_t adjusted_height, epoc::adapter::font_file_adapter_base *adapter) {
        // TODO: Proper conversion through physical screen size
        target_spec.height = adjusted_height * 15;

        // Set bitmap type that we gonna output
        static_cast<epoc::font_spec_v1 &>(target_spec).style.reset_flags();
        static_cast<epoc::font_spec_v1 &>(target_spec).style.set_glyph_bitmap_type(adapter->get_output_bitmap_type());
    }

    void fbscli::num_typefaces(service::ipc_context *ctx) {
        ctx->complete(static_cast<std::int32_t>(server<fbs_server>()->persistent_font_store.number_of_fonts()));
    }

    void fbscli::typeface_support(service::ipc_context *ctx) {
        std::optional<std::uint32_t> font_idx = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<epoc::typeface_support> support = ctx->get_argument_data_from_descriptor<epoc::typeface_support>(1);

        if (!support || !font_idx) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fbs_server *serv = server<fbs_server>();
        epoc::open_font_info *info = serv->persistent_font_store.seek_the_font_by_id(font_idx.value());

        if (!info) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        LOG_TRACE(SERVICE_FBS, "Basic information for Typeface support is provided, excluding number of heights and flags");

        const int approxiate_twips_mul = epoc::get_approximate_pixel_to_twips_mul(serv->kern->get_epoc_version());

        support->info_.name = info->face_attrib.name.to_std_string(nullptr);
        support->info_.flags = 0;
        support->max_height_in_twips_ = info->metrics.max_height * approxiate_twips_mul;
        support->min_height_in_twips_ = info->face_attrib.min_size_in_pixels * approxiate_twips_mul;
        support->num_heights_ = 1;
        support->is_scalable_ = info->adapter->vectorizable();

        if (info->face_attrib.style & epoc::open_font_face_attrib::serif) {
            support->info_.flags |= epoc::typeface_info::tf_serif;
        }

        if (!(info->face_attrib.style & epoc::open_font_face_attrib::mono_width)) {
            support->info_.flags |= epoc::typeface_info::tf_propotional;
        }

        if (info->face_attrib.style & epoc::open_font_face_attrib::symbol) {
            support->info_.flags |= epoc::typeface_info::tf_symbol;
        }

        ctx->write_data_to_descriptor_argument(1, support.value());
        ctx->complete(epoc::error_none);
    }

    template <typename T, typename Q>
    void fbscli::fill_bitmap_information(T *bmpfont, Q *of, epoc::open_font_info &info, epoc::font_spec_base &spec,
        kernel::process *font_user, const std::uint32_t desired_height, std::optional<std::pair<float, float>> scale_vector) {
        fbs_server *serv = server<fbs_server>();

        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            bmpfont->openfont = serv->host_ptr_to_guest_general_data(of).template cast<void>();
        } else {
            // Better make it offset for future debugging purpose
            // Mark bit 0 as set so that fntstore can recognised the offset model
            bmpfont->openfont = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(
                                                              of)
                                    - reinterpret_cast<std::uint8_t *>(bmpfont))
                | 0x1;
        }

        bmpfont->vtable = serv->fntstr_seg->relocate(font_user, serv->bmp_font_vtab.ptr_address());
        calculate_algorithic_style(bmpfont->algorithic_style, spec);

        float scale_factor_x = 0;
        float scale_factor_y = 0;

        if (!scale_vector) {
            if (desired_height != 0) {
                // Scale for max height
                scale_factor_x = scale_factor_y = static_cast<float>(desired_height) / info.metrics.max_height;
            } else {
                scale_factor_x = scale_factor_y = static_cast<float>(spec.height) / info.metrics.design_height;
            }
        } else {
            scale_factor_x = scale_vector->first;
            scale_factor_y = scale_vector->second;
        }

        do_scale_metrics(info.metrics, scale_factor_x, scale_factor_y);
        do_fill_bitmap_font_spec(bmpfont->spec_in_twips, spec, info.metrics.design_height, info.adapter);

        // Set font name and flags. Flags is stubbed
        info.scale_factor_x = scale_factor_x;
        info.scale_factor_y = scale_factor_y;

        static constexpr std::uint16_t MAX_TF_NAME = 24;
        bmpfont->spec_in_twips.tf.name.assign(nullptr, reinterpret_cast<std::uint8_t *>(info.face_attrib.name.data),
            MAX_TF_NAME * 2);
        bmpfont->spec_in_twips.tf.flags = spec.tf.flags;

        of->metrics = info.metrics;
        of->face_index_offset = static_cast<int>(info.idx);
        of->vtable = epoc::DEAD_VTABLE;
        of->allocator = epoc::DEAD_ALLOC;

        // Fill basic extended function info
        if constexpr (std::is_same_v<Q, epoc::open_font_v2>) {
            of->font_max_ascent = of->metrics.ascent;
            of->font_max_descent = of->metrics.descent;
            of->font_standard_descent = of->metrics.descent;
            of->font_captial_offset = 0;

            // Get the line gap!! This is no stub
            of->font_line_gap = static_cast<std::uint16_t>(info.adapter->line_gap(info.idx) * scale_factor_y);
        }

        // NOTE: Newer version (from S^3 onwards) uses offset. Older version just cast this directly to integer
        // Since I don't know the version that starts using offset yet, we just leave it be this for now
        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            of->session_cache_list_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_general_data(
                                                                              serv->session_cache_link)
                                                                          .ptr_address());
        } else {
            of->session_cache_list_offset = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(
                                                                          serv->session_cache_list)
                - reinterpret_cast<std::uint8_t *>(of));
        }

        if (serv->kern->is_eka1()) {
            // It allocs from the client-side using server heap the glyph cache.
            // We don't want this to happen, our heap is fake. And non existent
            // Alloc it by ourself
            epoc::open_font_glyph_cache_v1 *cache = serv->allocate_general_data<epoc::open_font_glyph_cache_v1>();

            if (!cache) {
                LOG_WARN(SERVICE_FBS, "Unable to supply empty cache for EKA1's open font!");
            } else {
                cache->entry_ = 0; // Purposedly make nullptr entry so that the cache is empty
                of->glyph_cache_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_general_data(cache)
                                                                       .ptr_address());
            }
        } else {
            of->glyph_cache_offset = 0;
        }
    }

    template <typename T>
    void fbs_server::destroy_bitmap_font(T *bmpfont) {
        // On EKA1, free the glyph cache offset
        if (legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
            epoc::open_font_v1 *ofo = reinterpret_cast<epoc::open_font_v1 *>(guest_general_data_to_host_ptr(bmpfont->openfont.template cast<std::uint8_t>()));

            if (ofo->glyph_cache_offset)
                free_general_data_impl(guest_general_data_to_host_ptr(ofo->glyph_cache_offset));
        }

        if (bmpfont->openfont.ptr_address() & 0x1) {
            // Better make it offset for future debugging purpose
            // Mark bit 0 as set so that fntstore can recognised the offset model
            free_general_data_impl(reinterpret_cast<std::uint8_t *>(bmpfont) + (bmpfont->openfont.ptr_address() & ~0x1));
        } else {
            free_general_data_impl(guest_general_data_to_host_ptr(bmpfont->openfont.template cast<std::uint8_t>()));
        }

        free_general_data(bmpfont);
    }

    template void fbscli::fill_bitmap_information<epoc::bitmapfont_v1, epoc::open_font_v1>(epoc::bitmapfont_v1 *bitmapfont, epoc::open_font_v1 *of, epoc::open_font_info &info, epoc::font_spec_base &spec,
        kernel::process *font_user, const std::uint32_t desired_height, std::optional<std::pair<float, float>> scale_vector);

    template void fbscli::fill_bitmap_information<epoc::bitmapfont_v2, epoc::open_font_v2>(epoc::bitmapfont_v2 *bitmapfont, epoc::open_font_v2 *of, epoc::open_font_info &info, epoc::font_spec_base &spec,
        kernel::process *font_user, const std::uint32_t desired_height, std::optional<std::pair<float, float>> scale_vector);

    template void fbs_server::destroy_bitmap_font<epoc::bitmapfont_v1>(epoc::bitmapfont_v1 *bmpfont);
    template void fbs_server::destroy_bitmap_font<epoc::bitmapfont_v2>(epoc::bitmapfont_v2 *bmpfont);

    epoc::bitmapfont_base *fbscli::create_bitmap_open_font(epoc::open_font_info &info, epoc::font_spec_base &spec, kernel::process *font_user, const std::uint32_t desired_height,
        std::optional<std::pair<float, float>> scale_vector) {
        fbs_server *serv = server<fbs_server>();
#define DO_BITMAP_OPEN_FONT_CREATION(ver)                                                      \
    epoc::open_font_v##ver *of = serv->allocate_general_data<epoc::open_font_v##ver>();        \
    if (!of) {                                                                                 \
        return nullptr;                                                                        \
    }                                                                                          \
    epoc::bitmapfont_v##ver *bmpfont = serv->allocate_general_data<epoc::bitmapfont_v##ver>(); \
    if (!bmpfont) {                                                                            \
        serv->free_general_data(of);                                                           \
        return nullptr;                                                                        \
    }                                                                                          \
    fill_bitmap_information<epoc::bitmapfont_v##ver>(bmpfont, of, info, spec, font_user,       \
        desired_height, scale_vector);                                                         \
    return bmpfont

        if (serv->kern->is_eka1()) {
            DO_BITMAP_OPEN_FONT_CREATION(1);
        }

        DO_BITMAP_OPEN_FONT_CREATION(2);
    }

    void fbscli::write_font_handle(service::ipc_context *ctx, fbsfont *font, const int index) {
        font_info result_info;

        result_info.handle = obj_table_.add(font);
        result_info.address_offset = font->guest_font_offset;
        result_info.server_handle = static_cast<std::int32_t>(font->id);

        ctx->write_data_to_descriptor_argument(index, result_info);
        ctx->complete(epoc::error_none);
    }

    void fbscli::get_nearest_font(service::ipc_context *ctx) {
        epoc::font_spec_v1 spec = *ctx->get_argument_data_from_descriptor<epoc::font_spec_v1>(0);

        // 1 x int of Max height - 2 x int of device size
        std::optional<eka2l1::vec3> size_info = ctx->get_argument_data_from_descriptor<eka2l1::vec3>(2);

        fbs_server *serv = server<fbs_server>();

        // NOTE: There's no consideration right now taken on nearest font in pixels vs in twips.
        const bool is_twips = is_opcode_ruler_twips(ctx->msg->function);
        const bool is_design_height = (serv->kern->is_eka1() || (!size_info.has_value()) || (ctx->msg->function == fbs_nearest_font_design_height_in_twips) || (ctx->msg->function == fbs_nearest_font_design_height_in_pixels));

        // For eka1, it's always in twips for spec height.
        // I don't know why when I tested with eka2, this height starts to be in pixels for pixel opcode.
        // TODO: Find out if spec height is always in twips for eka2.
        if (serv->kern->is_eka1() || is_twips) {
            spec.height = static_cast<std::int32_t>(static_cast<float>(spec.height) / epoc::get_approximate_pixel_to_twips_mul(serv->kern->get_epoc_version()));
        }

        // Observing font plugin on real phone, it seems to clamp the height between 2 to 256.
        static constexpr std::int32_t MAX_FONT_HEIGHT = 256;
        static constexpr std::int32_t MIN_FONT_HEIGHT = 2;

        spec.height = common::clamp<std::int32_t>(MIN_FONT_HEIGHT, MAX_FONT_HEIGHT, spec.height);

        if (spec.tf.name.get_length() == 0) {
            spec.tf.name = serv->default_system_font;
        }

        // Search the cache
        fbsfont *font = nullptr;
        epoc::open_font_info *ofi_suit = serv->persistent_font_store.seek_the_open_font(spec);

        if (!ofi_suit) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        static constexpr int max_acceptable_delta = 1;

        for (auto &font_obj : server<fbs_server>()->font_obj_container) {
            fbsfont *the_font = reinterpret_cast<fbsfont *>(font_obj.get());
            const std::int32_t delta = common::abs(is_design_height ? (spec.height - the_font->of_info.metrics.design_height) : (size_info->x - the_font->of_info.metrics.max_height));

            // Same adapter and font size is not to much of a difference
            if ((the_font->of_info.face_attrib.name.to_std_string(nullptr) == ofi_suit->face_attrib.name.to_std_string(nullptr))
                && (delta <= max_acceptable_delta)) {
                font = the_font;
                break;
            }
        }

        if (!font) {
            // Scale it
            font = serv->font_obj_container.make_new<fbsfont>();
            font->of_info = *ofi_suit;
            font->serv = serv;

            // If font is not vectorizable, keep the font as it's
            const bool vectorizable = font->of_info.adapter->vectorizable();
            std::uint32_t desired_height = (is_design_height || !vectorizable) ? 0 : size_info->x;

            epoc::bitmapfont_base *bmpfont = create_bitmap_open_font(font->of_info, spec, ctx->msg->own_thr->owning_process(),
                desired_height, vectorizable ? std::nullopt : std::make_optional(std::make_pair(1.0f, 1.0f)));

            if (!bmpfont) {
                ctx->complete(epoc::error_no_memory);
                return;
            }

            // S^3 warning!
            font->guest_font_offset = serv->host_ptr_to_guest_shared_offset(bmpfont);
        }

        write_font_handle(ctx, font, 1);
    }

    void fbscli::get_font_by_uid(service::ipc_context *ctx) {
        std::optional<epoc::uid> font_uid = ctx->get_argument_value<epoc::uid>(2);
        std::optional<epoc::alg_style> the_style = ctx->get_argument_data_from_descriptor<epoc::alg_style>(1);

        if (!the_style || !font_uid) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fbs_server *serv = server<fbs_server>();
        epoc::open_font_info *info = serv->persistent_font_store.seek_the_font_by_uid(font_uid.value());

        if (!info) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        epoc::font_spec_v1 the_spec;
        the_spec.tf.name = info->face_attrib.name.to_std_string(nullptr);
        the_spec.tf.flags = the_style->flags;

        the_spec.style.flags = the_style->flags;
        the_spec.height = info->metrics.design_height * the_style->height_factor;

        std::pair<float, float> scale_pair = { static_cast<float>(the_style->width_factor), static_cast<float>(the_style->height_factor) };

        fbsfont *font = nullptr;
        font = serv->font_obj_container.make_new<fbsfont>();
        font->of_info = *info;
        font->serv = serv;

        epoc::bitmapfont_base *bmpfont = create_bitmap_open_font(font->of_info, the_spec, ctx->msg->own_thr->owning_process(), 0, scale_pair);

        if (!bmpfont) {
            ctx->complete(epoc::error_no_memory);
            return;
        }

        // S^3 warning!
        font->guest_font_offset = serv->host_ptr_to_guest_shared_offset(bmpfont);
        write_font_handle(ctx, font, 0);
    }

    void fbscli::duplicate_font(service::ipc_context *ctx) {
        fbs_server *serv = server<fbs_server>();
        fbsfont *font = serv->font_obj_container.get<fbsfont>(*ctx->get_argument_value<epoc::handle>(0));

        if (!font) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        write_font_handle(ctx, font, 1);
    }

    void fbscli::get_twips_height(service::ipc_context *ctx) {
        fbs_server *serv = server<fbs_server>();
        std::optional<epoc::handle> font_local_handle = ctx->get_argument_value<epoc::handle>(0);

        if (!font_local_handle) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fbsfont *font = obj_table_.get<fbsfont>(font_local_handle.value());

        if (!font) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        const std::int32_t twips_height = static_cast<std::int32_t>(font->of_info.metrics.max_height *
            epoc::get_approximate_pixel_to_twips_mul(serv->kern->get_epoc_version()));

        ctx->write_data_to_descriptor_argument<std::int32_t>(1, twips_height);
        ctx->complete(epoc::error_none);
    }

    fbsfont *fbscli::get_font_object(service::ipc_context *ctx) {
        if ((ver_.build > 94) || (server<fbs_server>()->get_system()->get_symbian_version_use() >= epocver::epoc95)) {
            // Use object table handle
            return obj_table_.get<fbsfont>(*ctx->get_argument_value<epoc::handle>(0));
        }

        const eka2l1::address addr = *ctx->get_argument_value<eka2l1::address>(0);
        return server<fbs_server>()->look_for_font_with_address(addr);
    }

    void fbscli::get_face_attrib(service::ipc_context *ctx) {
        // Look for bitmap font with this same handle
        const fbsfont *font = get_font_object(ctx);

        if (!font) {
            ctx->complete(false);
            return;
        }

        ctx->write_data_to_descriptor_argument(1, font->of_info.face_attrib);
        ctx->complete(true);
    }

    void fbscli::rasterize_glyph(service::ipc_context *ctx) {
        const std::uint32_t codepoint = *ctx->get_argument_value<std::uint32_t>(1);
        const fbsfont *font = get_font_object(ctx);

        process_ptr own_pr = ctx->msg->own_thr->owning_process();
        epoc::bitmapfont_base *bmp_font = reinterpret_cast<epoc::bitmapfont_base *>(font->guest_font_offset + reinterpret_cast<std::uint8_t *>(server<fbs_server>()->shared_chunk->host_base()));

        if (codepoint & 0x80000000) {
            //LOG_DEBUG(SERVICE_FBS, "Trying to rasterize glyph index {}", codepoint & ~0x80000000);
        } else {
            //LOG_DEBUG(SERVICE_FBS, "Trying to rasterize character '{}' (code {})", static_cast<char>(codepoint), codepoint);
        }

        int rasterized_width = 0;
        int rasterized_height = 0;

        const epoc::open_font_info *info = &(font->of_info);
        epoc::glyph_bitmap_type bitmap_type = epoc::glyph_bitmap_type::default_glyph_bitmap;
        std::uint32_t bitmap_data_size = 0;

        // Get server font handle
        // The returned bitmap is 8bpp single channel. Luckily Symbian likes this (at least in v3 and upper).
        std::uint8_t *bitmap_data = info->adapter->get_glyph_bitmap(info->idx, codepoint, font->of_info.metrics.max_height,
            &rasterized_width, &rasterized_height, bitmap_data_size, &bitmap_type);

        if (!bitmap_data && !info->adapter->does_glyph_exist(info->idx, codepoint)) {
            // The glyph is not available. Let the client know. With code 0, we already use '?'
            // On S^3, it expect us to return false here.
            // On lower version, it expect us to return nullptr, so use 0 here is for the best.
            ctx->complete(0);
            return;
        }

        // Add it to session cache
        fbs_server *serv = server<fbs_server>();
        kernel::process *pr = ctx->msg->own_thr->owning_process();

#define MAKE_CACHE_ENTRY(entry_ver, type)                                                                                                   \
    epoc::open_font_session_cache_entry_v##entry_ver *cache_entry = reinterpret_cast<decltype(cache_entry)>(                                \
        serv->allocate_general_data_impl(sizeof(epoc::open_font_session_cache_entry_v##entry_ver) + bitmap_data_size + 1));                 \
    cache_entry->codepoint = codepoint;                                                                                                     \
    cache_entry->glyph_index = codepoint % session_cache->offset_array.offset_array_count;                                                  \
    cache_entry->offset = sizeof(epoc::open_font_session_cache_entry_v##entry_ver) + 1;                                                     \
    info->adapter->get_glyph_metric(info->idx, codepoint, cache_entry->metric,                                                              \
        reinterpret_cast<type *>(bmp_font)->algorithic_style.baseline_offsets_in_pixel,                                                     \
        font->of_info.metrics.max_height);                                                                                                  \
    cache_entry->metric.width = rasterized_width;                                                                                           \
    cache_entry->metric.height = rasterized_height;                                                                                         \
    cache_entry->metric.bitmap_type = bitmap_type;                                                                                          \
    const auto cache_entry_ptr = serv->host_ptr_to_guest_general_data(cache_entry).ptr_address();                                           \
    if (epoc::does_client_use_pointer_instead_of_offset(this)) {                                                                            \
        cache_entry->font_offset = static_cast<std::int32_t>(reinterpret_cast<type *>(bmp_font)->openfont.ptr_address());                   \
    } else {                                                                                                                                \
        cache_entry->font_offset = static_cast<std::int32_t>(reinterpret_cast<type *>(bmp_font)->openfont.ptr_address() - cache_entry_ptr); \
    }                                                                                                                                       \
    std::memcpy(reinterpret_cast<std::uint8_t *>(cache_entry) + cache_entry->offset, bitmap_data,                                           \
        bitmap_data_size);                                                                                                                  \
    info->adapter->free_glyph_bitmap(bitmap_data);                                                                                          \
    if (epoc::does_client_use_pointer_instead_of_offset(this)) {                                                                            \
        cache_entry->offset += static_cast<std::int32_t>(cache_entry_ptr);                                                                  \
    }

        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            // Use linked
            epoc::open_font_session_cache_link *link = serv->session_cache_link->get_or_create(this);
            epoc::open_font_session_cache_old *session_cache = link->cache.get(pr);

            if (serv->kern->is_eka1()) {
                MAKE_CACHE_ENTRY(1, epoc::bitmapfont_v1);

                session_cache->add_glyph<epoc::open_font_session_cache_entry_v1>(this, codepoint, cache_entry);

                cache_entry->last_use = session_cache->last_use_counter++;

                glyph_info_for_legacy_return_->codepoint = codepoint;
                glyph_info_for_legacy_return_->metric_offset = serv->host_ptr_to_guest_general_data(&cache_entry->metric).ptr_address();
                glyph_info_for_legacy_return_->offset = cache_entry->offset;

                ctx->complete(glyph_info_for_legacy_return_addr_);
                return;
            } else {
                MAKE_CACHE_ENTRY(2, epoc::bitmapfont_v2);

                session_cache->add_glyph<epoc::open_font_session_cache_entry_v2>(this, codepoint, cache_entry);

                cache_entry->last_use = session_cache->last_use_counter++;
                cache_entry->metric_offset = serv->host_ptr_to_guest_general_data(&cache_entry->metric).ptr_address();

                ctx->complete(cache_entry_ptr);
                return;
            }
        }

        epoc::open_font_session_cache_v3 *session_cache = serv->session_cache_list->get(this,
            static_cast<std::int32_t>(connection_id_), true);

        MAKE_CACHE_ENTRY(3, epoc::bitmapfont_v2);
        session_cache->add_glyph(this, codepoint, cache_entry);

        // From S^3 onwards, the 2nd argument contains some necessary struct we need to fill in (metrics offset
        // and bitmap pointer offset) so we don't have to lookup anymore. On older version, the 2nd argument is
        // zero. We can do a check.
        struct rasterize_param {
            std::int32_t metrics_offset;
            std::int32_t bitmap_offset;
        };

        if (*ctx->get_argument_value<std::int32_t>(2) != 0) {
            // We can write rasterize param in there.
            rasterize_param param;
            param.metrics_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_shared_offset(&cache_entry->metric));
            param.bitmap_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_shared_offset(
                reinterpret_cast<std::uint8_t *>(cache_entry) + cache_entry->offset));

            ctx->write_data_to_descriptor_argument<rasterize_param>(2, param);
        }

        // Success, set to true on S^3
        ctx->complete(true);
    }

    void fbscli::set_default_glyph_bitmap_type(service::ipc_context *ctx) {
        std::optional<std::uint32_t> default_type = ctx->get_argument_value<std::uint32_t>(0);
        if (!default_type.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        server<fbs_server>()->set_default_glyph_bitmap_type(static_cast<epoc::glyph_bitmap_type>(default_type.value()));
        ctx->complete(epoc::error_none);
    }

    void fbscli::get_default_glyph_bitmap_type(service::ipc_context *ctx) {
        ctx->complete(static_cast<int>(server<fbs_server>()->get_default_glyph_bitmap_type()));
    }

    void fbscli::has_character(service::ipc_context *ctx) {
        // Look for bitmap font with this same handle
        const fbsfont *font = get_font_object(ctx);

        if (!font) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::optional<std::int32_t> code = ctx->get_argument_value<std::int32_t>(1);
        if (!code.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::int32_t result = static_cast<std::int32_t>(font->of_info.adapter->has_character(font->of_info.idx, code.value()));
        ctx->complete(result);
    }

    void fbscli::get_font_shaping(service::ipc_context *ctx) {
        fbsfont *font = get_font_object(ctx);

        if (!font) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::optional<epoc::open_font_shaping_parameter> params = ctx->get_argument_data_from_descriptor<epoc::open_font_shaping_parameter>(2);
        if (!params.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::optional<std::u16string> text_to_shape = ctx->get_argument_value<std::u16string>(1);
        if (!text_to_shape.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        // Open a temporary header to calculate neccessary allocations
        epoc::open_font_shaping_header temp_header;
        if (!font->of_info.adapter->make_text_shape(font->of_info.idx, params.value(), text_to_shape.value(), font->of_info.metrics.max_height, temp_header, nullptr)) {
            ctx->complete(epoc::error_general);
            return;
        }

        fbs_server *serv = server<fbs_server>();
        std::uint8_t *allocated_data = reinterpret_cast<std::uint8_t*>(serv->allocate_general_data_impl(sizeof(epoc::open_font_shaping_header) + temp_header.glyph_count_ * 10 + 4));
        if (!allocated_data) {
            LOG_TRACE(SERVICE_FBS, "Can't allocate data for store shaping!");
            ctx->complete(epoc::error_no_memory);
            return;
        }

        if (!font->of_info.adapter->make_text_shape(font->of_info.idx, params.value(), text_to_shape.value(), font->of_info.metrics.max_height, *reinterpret_cast<epoc::open_font_shaping_header*>(allocated_data),
            allocated_data + sizeof(epoc::open_font_shaping_header))) {
            serv->free_general_data_impl(allocated_data);
            ctx->complete(epoc::error_general);
            return;
        }

        // Push to cleanup list
        font->shapings.push_back(allocated_data);

        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            ctx->complete(static_cast<int>(serv->host_ptr_to_guest_general_data(allocated_data).ptr_address()));
        } else {
            ctx->complete(static_cast<int>(serv->host_ptr_to_guest_shared_offset(allocated_data)));
        }
    }

    void fbscli::delete_font_shaping(service::ipc_context *ctx) {
        fbsfont *font = get_font_object(ctx);
        std::optional<std::int32_t> offset = ctx->get_argument_value<std::int32_t>(1);

        if (!font || !offset.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fbs_server *serv = server<fbs_server>();
        std::uint8_t *shape_ptr = nullptr;
        
        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            std::uint32_t ptr_addr = static_cast<std::uint32_t>(offset.value());
            shape_ptr = reinterpret_cast<std::uint8_t*>(serv->guest_general_data_to_host_ptr(ptr<std::uint8_t>(ptr_addr)));
        } else {
            shape_ptr = serv->get_shared_chunk_base() + offset.value();
        }

        for (std::size_t i = 0; i < font->shapings.size(); i++) {
            if (shape_ptr == font->shapings[i]) {
                serv->free_general_data_impl(shape_ptr);
                font->shapings.erase(font->shapings.begin() + i);

                break;
            }
        }

        ctx->complete(epoc::error_none);
    }

    fbsfont::~fbsfont() {
        // Free atlas + bitmap
        atlas.destroy(serv->get_graphics_driver());
        std::uint8_t *font_ptr = serv->get_shared_chunk_base() + guest_font_offset;

        switch (serv->legacy_level()) {
        case FBS_LEGACY_LEVEL_KERNEL_TRANSITION:
        case FBS_LEGACY_LEVEL_EARLY_KERNEL_TRANSITION:
        case FBS_LEGACY_LEVEL_S60V1:
            serv->destroy_bitmap_font<epoc::bitmapfont_v1>(reinterpret_cast<epoc::bitmapfont_v1 *>(font_ptr));
            break;

        default:
            serv->destroy_bitmap_font<epoc::bitmapfont_v2>(reinterpret_cast<epoc::bitmapfont_v2 *>(font_ptr));
            break;
        }

        for (std::size_t i = 0; i < shapings.size(); i++) {
            serv->free_general_data_impl(shapings[i]);
        }
    }

    fbsfont *fbs_server::get_font(const service::uid id) {
        return font_obj_container.get<fbsfont>(id);
    }

    void fbs_server::load_fonts_from_directory(eka2l1::io_system *io, eka2l1::directory *folder) {
        while (auto entry = folder->get_next_entry()) {
            symfile f = io->open_file(common::utf8_to_ucs2(entry->full_path), READ_MODE | BIN_MODE);
            const std::uint64_t fsize = f->size();

            std::vector<std::uint8_t> buf;

            buf.resize(fsize);
            f->read_file(&buf[0], 1, static_cast<std::uint32_t>(buf.size()));

            f->close();

            // Add fonts
            const auto extension = common::lowercase_string(eka2l1::path_extension(entry->full_path));
            epoc::adapter::font_file_adapter_kind adapter_kind = epoc::adapter::font_file_adapter_kind::none;

            if (extension == ".ttf") {
                adapter_kind = epoc::adapter::font_file_adapter_kind::stb;
            } else if (extension == ".gdr") {
                adapter_kind = epoc::adapter::font_file_adapter_kind::gdr;
            }

            if (adapter_kind != epoc::adapter::font_file_adapter_kind::none) {
                persistent_font_store.add_fonts(buf, adapter_kind);
            }
        }
    }

    void fbs_server::load_fonts(eka2l1::io_system *io) {
        // Search all drives
        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
            if (io->get_drive_entry(drv)) {
                const std::u16string fonts_folder_path = std::u16string{ drive_to_char16(drv) } + (kern->is_eka1() ? u":\\System\\Fonts\\" : u":\\Resource\\Fonts\\");
                auto folder = io->open_dir(fonts_folder_path, {}, io_attrib_include_file);

                if (folder) {
                    LOG_TRACE(SERVICE_FBS, "Found font folder: {}", common::ucs2_to_utf8(fonts_folder_path));
                    load_fonts_from_directory(io, folder.get());
                }
                // TODO: Implement FS callback
            }
        }
    }
}
