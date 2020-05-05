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
#include <common/log.h>
#include <common/vecx.h>

#include <epoc/epoc.h>
#include <epoc/vfs.h>

#include <epoc/utils/err.h>

namespace eka2l1::epoc {
    bool does_client_use_pointer_instead_of_offset(fbscli *cli) {
        return cli->client_version().build <= epoc::RETURN_POINTER_NOT_OFFSET_BUILD_LIMIT;
    }

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

        if (!offset_ptr && create) {
            // Create new one.
            open_font_session_cache_v3 *cache = serv->allocate_general_data<open_font_session_cache_v3>();
            const std::int32_t offset = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(cache) - reinterpret_cast<std::uint8_t *>(this));

            cache->session_handle = session_handle;
            cache->random_seed = 0;

            // Allocate offset array
            cache->offset_array.init(cli, NORMAL_SESSION_CACHE_ENTRY_COUNT);

            add(session_handle, offset);
            return cache;
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

        open_font_session_cache_v2 *cache = serv->allocate_general_data<open_font_session_cache_v2>();

        cache->session_handle = cli->connection_id_;
        cache->offset_array.init(cli, NORMAL_SESSION_CACHE_ENTRY_COUNT);
        cache->last_use_counter = 0;

        // Set the cache guest pointer
        next_link->cache = serv->host_ptr_to_guest_general_data(cache).cast<epoc::open_font_session_cache_v2>();

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
                current_cache->destroy(cli);

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

    void open_font_glyph_v2::destroy(fbs_server *serv) {
        // Nothing.... May add something here later
    }

    void open_font_glyph_v3::destroy(fbs_server *serv) {
        // Nothing.... May add something here later
    }

#define OPEN_FONT_SESSION_CACHE_DESTROY_IMPL(version)                        \
    void open_font_session_cache_v##version::destroy(fbscli *cli) {          \
        for (std::int32_t i = 0; i < offset_array.offset_array_count; i++) { \
            if (!offset_array.is_entry_empty(cli, i)) {                      \
                auto glyph_cache = offset_array.get_glyph(cli, i);           \
                cli->server<fbs_server>()->free_general_data(glyph_cache);   \
            }                                                                \
        }                                                                    \
    }

    OPEN_FONT_SESSION_CACHE_DESTROY_IMPL(2)
    OPEN_FONT_SESSION_CACHE_DESTROY_IMPL(3)

    void open_font_session_cache_v3::add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph) {
        const std::uint32_t real_index = (code & 0x7fffffff) % offset_array.offset_array_count;

        if (!offset_array.is_entry_empty(cli, real_index)) {
            auto glyph_cache = offset_array.get_glyph(cli, real_index);
            cli->server<fbs_server>()->free_general_data(glyph_cache);
        }

        offset_array.set_glyph(cli, real_index, the_glyph);
    }

    void open_font_session_cache_v2::add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph) {
        std::uint32_t real_index = (code & 0x7fffffff) % offset_array.offset_array_count;

        if (!offset_array.is_entry_empty(cli, real_index)) {
            // Search for the least last used
            auto ptr = offset_array.pointer(cli);
            fbs_server *serv = cli->server<fbs_server>();
            memory_system *mem = serv->get_system()->get_memory_system();

            std::int32_t least_use = 99999999;

            for (std::int32_t i = 0; i < offset_array.offset_array_count; i++) {
                if (!ptr[i]) {
                    real_index = i;
                    break;
                }

                auto entry = eka2l1::ptr<open_font_session_cache_entry_v2>(ptr[i]).get(mem);

                if (entry->last_use < least_use) {
                    least_use = entry->last_use;
                    real_index = i;
                }
            }

            // Free at given address.
            auto glyph_cache = offset_array.get_glyph(cli, real_index);
            cli->server<fbs_server>()->free_general_data(glyph_cache);
        }

        offset_array.set_glyph(cli, real_index, the_glyph);
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
        for (auto &font_cache_obj_ptr : font_obj_container) {
            fbsfont *temp_font_ptr = reinterpret_cast<fbsfont *>(font_cache_obj_ptr.get());

            if (temp_font_ptr && temp_font_ptr->guest_font_handle.ptr_address() == addr) {
                return temp_font_ptr;
            }
        }

        return nullptr;
    }

    static void do_scale_metrics(epoc::open_font_metrics &metrics, const float scale) {
        metrics.max_height = static_cast<std::int16_t>(metrics.max_height * scale);
        metrics.ascent = static_cast<std::int16_t>(metrics.ascent * scale);
        metrics.descent = static_cast<std::int16_t>(metrics.descent * scale);
        metrics.design_height = static_cast<std::int16_t>(metrics.design_height * scale);
        metrics.max_depth = static_cast<std::int16_t>(metrics.max_depth * scale);
        metrics.max_width = static_cast<std::int16_t>(metrics.max_width * scale);
    }

    static std::int32_t calculate_baseline(epoc::font_spec &spec) {
        if (spec.style.flags & epoc::font_style::super) {
            // Superscript, see 2^5 for example, 5 is the superscript
            constexpr std::int32_t super_script_offset_percentage = -28;
            return super_script_offset_percentage * spec.height / 100;
        }

        if (spec.style.flags & epoc::font_style::sub) {
            // Subscript, it's the opposite with superscript. HNO3 for example, in chemistry,
            // 3 supposed to be below HNO
            constexpr std::int32_t subscript_offset_percentage = 14;
            return subscript_offset_percentage * spec.height / 100;
        }

        // Nothing special... Move along.
        return 0;
    }

    static void calculate_algorithic_style(epoc::alg_style &style, epoc::font_spec &spec) {
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
    static void do_fill_bitmap_font_spec(epoc::font_spec &target_spec, epoc::font_spec &given_spec,
        std::int16_t adjusted_height, epoc::adapter::font_file_adapter_base *adapter) {
        // TODO: Proper conversion through physical screen size
        target_spec.height = adjusted_height * 15;

        // Set bitmap type that we gonna output
        target_spec.style.reset_flags();
        target_spec.style.set_glyph_bitmap_type(adapter->get_output_bitmap_type());
    }

    void fbscli::num_typefaces(service::ipc_context *ctx) {
        ctx->set_request_status(static_cast<std::int32_t>(server<fbs_server>()->
            persistent_font_store.number_of_fonts()));
    }

    void fbscli::typeface_support(service::ipc_context *ctx) {
        
    }

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
            ctx->set_request_status(epoc::error_not_found);
        }

        // Scale it
        epoc::open_font *of = serv->allocate_general_data<epoc::open_font>();
        epoc::bitmapfont *bmpfont = server<fbs_server>()->allocate_general_data<epoc::bitmapfont>();

        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            bmpfont->openfont = serv->host_ptr_to_guest_general_data(of).cast<void>();
        } else {
            // Better make it offset for future debugging purpose
            bmpfont->openfont = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(
                                                              of)
                - reinterpret_cast<std::uint8_t *>(bmpfont));
        }

        bmpfont->vtable = serv->bmp_font_vtab;
        calculate_algorithic_style(bmpfont->algorithic_style, spec);

        font = serv->font_obj_container.make_new<fbsfont>();

        // S^3 warning!
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

        do_scale_metrics(font->of_info.metrics, scale_factor);
        do_fill_bitmap_font_spec(bmpfont->spec_in_twips, spec, font->of_info.metrics.design_height,
            font->of_info.adapter);

        font->of_info.scale_factor_x = scale_factor;
        font->of_info.scale_factor_y = scale_factor;

        font->serv = serv;

        of->metrics = font->of_info.metrics;
        of->face_index_offset = static_cast<int>(ofi_suit->idx);
        of->vtable = epoc::DEAD_VTABLE;

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

        // TODO (pent0): Fill basic font info to epoc::open_font

        font_info result_info;

        result_info.handle = obj_table_.add(font);
        result_info.address_offset = font->guest_font_handle.ptr_address() - serv->shared_chunk->base().ptr_address();
        result_info.server_handle = static_cast<std::int32_t>(font->id);

        ctx->write_arg_pkg(1, result_info);
        ctx->set_request_status(epoc::error_none);
    }

    void fbscli::duplicate_font(service::ipc_context *ctx) {
        fbs_server *serv = server<fbs_server>();
        fbsfont *font = serv->font_obj_container.get<fbsfont>(*ctx->get_arg<epoc::handle>(0));

        if (!font) {
            ctx->set_request_status(epoc::error_not_found);
            return;
        }

        font_info result_info;

        // Add new one
        result_info.handle = obj_table_.add(font);
        result_info.address_offset = font->guest_font_handle.ptr_address() - serv->shared_chunk->base().ptr_address();
        result_info.server_handle = static_cast<std::int32_t>(font->id);

        ctx->write_arg_pkg(1, result_info);
        ctx->set_request_status(epoc::error_none);
    }

    fbsfont *fbscli::get_font_object(service::ipc_context *ctx) {
        if (ver_.build > 94) {
            // Use object table handle
            return server<fbs_server>()->font_obj_container.get<fbsfont>(*ctx->get_arg<epoc::handle>(0));
        }

        const eka2l1::address addr = *ctx->get_arg<eka2l1::address>(0);
        return server<fbs_server>()->look_for_font_with_address(addr);
    }

    void fbscli::get_face_attrib(service::ipc_context *ctx) {
        // Look for bitmap font with this same handle
        const fbsfont *font = get_font_object(ctx);

        if (!font) {
            ctx->set_request_status(false);
            return;
        }

        ctx->write_arg_pkg(1, font->of_info.face_attrib);
        ctx->set_request_status(true);
    }

    void fbscli::rasterize_glyph(service::ipc_context *ctx) {
        const std::uint32_t codepoint = *ctx->get_arg<std::uint32_t>(1);
        const fbsfont *font = get_font_object(ctx);
        const eka2l1::address addr = font->guest_font_handle.ptr_address();

        if (codepoint & 0x80000000) {
            LOG_DEBUG("Trying to rasterize glyph index {}", codepoint & ~0x80000000);
        } else {
            LOG_DEBUG("Trying to rasterize character '{}' (code {})", static_cast<char>(codepoint), codepoint);
        }

        int rasterized_width = 0;
        int rasterized_height = 0;

        const epoc::open_font_info *info = &(font->of_info);
        epoc::glyph_bitmap_type bitmap_type = epoc::glyph_bitmap_type::default_glyph_bitmap;

        // Get server font handle
        // The returned bitmap is 8bpp single channel. Luckly Symbian likes this. (at least in v3 and upper).
        std::uint8_t *bitmap_data = info->adapter->get_glyph_bitmap(info->idx, codepoint, info->scale_factor_x,
            info->scale_factor_y, &rasterized_width, &rasterized_height, &bitmap_type);

        const std::size_t bitmap_data_size = rasterized_height * rasterized_width;

        if (!bitmap_data) {
            // The glyph is not available. Let the client know. With code 0, we already use '?'
            // On S^3, it expect us to return false here.
            // On lower version, it expect us to return nullptr, so use 0 here is for the best.
            ctx->set_request_status(0);
            return;
        }

        // Add it to session cache
        fbs_server *serv = server<fbs_server>();
        kernel::process *pr = ctx->msg->own_thr->owning_process();

#define MAKE_CACHE_ENTRY(entry_ver)                                                                                         \
    epoc::open_font_session_cache_entry_v##entry_ver *cache_entry = reinterpret_cast<decltype(cache_entry)>(                \
        serv->allocate_general_data_impl(sizeof(epoc::open_font_session_cache_entry_v##entry_ver) + bitmap_data_size + 1)); \
    epoc::bitmapfont *bmp_font = eka2l1::ptr<epoc::bitmapfont>(addr).get(pr);                                               \
    cache_entry->codepoint = codepoint;                                                                                     \
    cache_entry->glyph_index = codepoint % session_cache->offset_array.offset_array_count;                                  \
    cache_entry->offset = sizeof(epoc::open_font_session_cache_entry_v##entry_ver) + 1;                                     \
    info->adapter->get_glyph_metric(info->idx, codepoint, cache_entry->metric,                                              \
        bmp_font->algorithic_style.baseline_offsets_in_pixel, info->scale_factor_x, info->scale_factor_y);                  \
    cache_entry->metric.width = rasterized_width;                                                                           \
    cache_entry->metric.height = rasterized_height;                                                                         \
    cache_entry->metric.bitmap_type = bitmap_type;                                                                          \
    const auto cache_entry_ptr = serv->host_ptr_to_guest_general_data(cache_entry).ptr_address();                           \
    if (epoc::does_client_use_pointer_instead_of_offset(this)) {                                                            \
        cache_entry->font_offset = static_cast<std::int32_t>(bmp_font->openfont.ptr_address());                             \
    } else {                                                                                                                \
        cache_entry->font_offset = static_cast<std::int32_t>(bmp_font->openfont.ptr_address() - cache_entry_ptr);           \
    }                                                                                                                       \
    std::memcpy(reinterpret_cast<std::uint8_t *>(cache_entry) + cache_entry->offset, bitmap_data,                           \
        bitmap_data_size);                                                                                                  \
    info->adapter->free_glyph_bitmap(bitmap_data);                                                                          \
    if (epoc::does_client_use_pointer_instead_of_offset(this)) {                                                            \
        cache_entry->offset += static_cast<std::int32_t>(cache_entry_ptr);                                                  \
    }                                                                                                                       \
    session_cache->add_glyph(this, codepoint, cache_entry)

        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            // Use linked
            epoc::open_font_session_cache_link *link = serv->session_cache_link->get_or_create(this);
            epoc::open_font_session_cache_v2 *session_cache = link->cache.get(pr);

            MAKE_CACHE_ENTRY(2);

            cache_entry->last_use = session_cache->last_use_counter++;

            // What the hell?
            cache_entry->metric_offset = serv->host_ptr_to_guest_general_data(&cache_entry->metric).ptr_address();

            // Return the pointer to glyph cache entry on lower version
            ctx->set_request_status(cache_entry_ptr);
            return;
        }

        epoc::open_font_session_cache_v3 *session_cache = serv->session_cache_list->get(this,
            static_cast<std::int32_t>(connection_id_), true);

        MAKE_CACHE_ENTRY(3);

        // From S^3 onwards, the 2nd argument contains some neccessary struct we need to fill in (metrics offset
        // and bitmap pointer offset) so we don't have to lookup anymore. On older version, the 2nd argument is
        // zero. We can do a check.
        struct rasterize_param {
            std::int32_t metrics_offset;
            std::int32_t bitmap_offset;
        };

        if (*ctx->get_arg<std::int32_t>(2) != 0) {
            // We can write rasterize param in there.
            rasterize_param param;
            param.metrics_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_shared_offset(&cache_entry->metric));
            param.bitmap_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_shared_offset(
                reinterpret_cast<std::uint8_t *>(cache_entry) + cache_entry->offset));

            ctx->write_arg_pkg<rasterize_param>(2, param);
        }

        // Success, set to true on S^3
        ctx->set_request_status(true);
    }

    void fbsfont::deref() {
        if (count == 1) {
            atlas.free(serv->get_graphics_driver());
        }

        epoc::ref_count_object::deref();
    }

    fbsfont *fbs_server::get_font(const service::uid id) {
        return font_obj_container.get<fbsfont>(id);
    }

    void fbs_server::load_fonts(eka2l1::io_system *io) {
        // Search all drives
        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
            if (io->get_drive_entry(drv)) {
                const std::u16string fonts_folder_path = std::u16string{ drive_to_char16(drv) } + u":\\Resource\\Fonts\\*.ttf";
                auto folder = io->open_dir(fonts_folder_path, io_attrib::include_file);

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
