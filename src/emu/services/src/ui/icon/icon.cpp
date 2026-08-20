/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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
#include <services/ui/icon/icon.h>
#include <services/ui/icon/ops.h>

#include <services/fbs/bitmap.h>

#include <common/buffer.h>
#include <common/cvt.h>
#include <loader/mbm.h>
#include <loader/mif.h>
#include <loader/nvg.h>
#include <system/epoc.h>
#include <utils/err.h>
#include <vfs/vfs.h>

#include <lunasvg.h>

#include <cstring>
#include <memory>

namespace eka2l1 {
    akn_icon_server_session::akn_icon_server_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version version)
        : service::typical_session(svr, client_ss_uid, version) {
    }

    void akn_icon_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case akn_icon_server_get_init_data: {
            // Write initialisation data to buffer
            ctx->write_data_to_descriptor_argument<epoc::akn_icon_init_data>(0, *server<akn_icon_server>()->get_init_data(), nullptr, true);
            ctx->complete(epoc::error_none);

            break;
        }

        case akn_icon_server_retrieve_or_create_shared_icon: {
            server<akn_icon_server>()->retrieve_icon(ctx);
            break;
        }

        case akn_icon_server_free_bitmap: {
            server<akn_icon_server>()->free_bitmap(ctx);
            break;
        }

        // The real server keeps the decoded source data of an icon around so a later
        // resize does not have to read the container again, and hands it back when the
        // client is done. This server re-renders from the container file every time and
        // caches the results in "icons", so there is nothing to pin or hand back: both
        // requests are already satisfied, and so is a request to enable the cache.
        case akn_icon_server_preserve_icon_data:
        case akn_icon_server_destroy_icon_data:
        case akn_icon_server_request_to_enable_cache: {
            ctx->complete(epoc::error_none);
            break;
        }

        default: {
            // Complete even what we don't implement. A client blocks in SendReceive
            // until the server answers, so leaving a message hanging freezes the
            // calling thread for good - one unknown opcode is enough to leave every
            // app on a device drawing nothing.
            LOG_ERROR(SERVICE_UI, "Unimplemented IPC opcode for AknIconServer session: 0x{:X}", ctx->msg->function);
            ctx->complete(epoc::error_not_supported);

            break;
        }
        }
    }

    akn_icon_server::akn_icon_server(eka2l1::system *sys)
        : service::typical_server(sys, "!AknIconServer") {
    }

    void akn_icon_server::connect(service::ipc_context &context) {
        if (!(flags & akn_icon_srv_flag_inited)) {
            init_server();
        }

        create_session<akn_icon_server_session>(&context);
        context.complete(epoc::error_none);
    }

    // Open an icon container, resolving bare names (e.g. "Calcsoft.mif") against the
    // standard resource locations the way AknIconServer would.
    static symfile open_icon_container(io_system *io, const std::u16string &path) {
        symfile f = io->open_file(path, READ_MODE | BIN_MODE);
        if (f) {
            return f;
        }

        // No drive/dir component? Search the well-known resource folders on every drive.
        if ((path.find(u':') == std::u16string::npos) && (path.find(u'\\') == std::u16string::npos)) {
            static const char16_t *kPrefixes[] = { u"\\resource\\apps\\", u"\\resource\\" };
            for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(drv - 1)) {
                auto entry = io->get_drive_entry(drv);
                if (!entry) {
                    continue;
                }
                for (const char16_t *prefix : kPrefixes) {
                    std::u16string candidate;
                    candidate += drive_to_char16(drv);
                    candidate += u":";
                    candidate += prefix;
                    candidate += path;
                    f = io->open_file(candidate, READ_MODE | BIN_MODE);
                    if (f) {
                        return f;
                    }
                }
            }
        }

        return nullptr;
    }

    // Load a .mif icon entry (SVG / NVG vector) into a renderable lunasvg document.
    static std::unique_ptr<lunasvg::Document> load_mif_icon_document(io_system *io,
        const std::u16string &path, const int icon_index, const int want_w, const int want_h) {
        symfile f = open_icon_container(io, path);
        if (!f) {
            return nullptr;
        }

        ro_file_stream rfs(f.get());
        loader::mif_file parser(reinterpret_cast<common::ro_stream *>(&rfs));
        if (!parser.do_parse()) {
            return nullptr;
        }

        int dest_size = 0;
        if (!parser.read_mif_entry(icon_index, nullptr, dest_size) || (dest_size <= 0)) {
            return nullptr;
        }

        std::vector<std::uint8_t> data(dest_size);
        if (!parser.read_mif_entry(icon_index, data.data(), dest_size)) {
            return nullptr;
        }

        common::wo_growable_buf_stream svg_out;
        loader::nvg_options opts;
        opts.width = want_w;
        opts.height = want_h;

        if (!loader::convert_mif_icon_to_svg(data.data(), data.size(), svg_out, &opts)) {
            return nullptr;
        }

        const std::string svg_content = svg_out.content();
        if (svg_content.empty()) {
            return nullptr;
        }

        return lunasvg::Document::loadFromData(svg_content);
    }

    // Write a rendered RGBA buffer into a colour bitmap and its alpha mask, both
    // already created at (w, h). The server's configured icon mode decides the
    // colour depth, and the mask is EGray256 or EGray2 depending on the ROM's own
    // AknIcon configuration resource.
    static void blit_rgba_into_icon(fbs_server *fbss, epoc::bitwise_bitmap *colour,
        epoc::bitwise_bitmap *mask, const std::uint8_t *rgba, const int w, const int h) {
        std::uint8_t *cdata = colour ? colour->data_pointer(fbss) : nullptr;
        std::uint8_t *mdata = mask ? mask->data_pointer(fbss) : nullptr;
        const int cbw = colour ? colour->byte_width_ : 0;
        const int mbw = mask ? mask->byte_width_ : 0;
        const epoc::display_mode cmode = colour ? colour->settings_.current_display_mode() : epoc::display_mode::none;
        const int cbpp = epoc::get_bpp_from_display_mode(cmode);
        const epoc::display_mode mmode = mask ? mask->settings_.current_display_mode() : epoc::display_mode::none;
        const int mbpp = mask ? epoc::get_bpp_from_display_mode(mmode) : 0;

        // A 1bpp mask is stored as 32-bit words, one bit per pixel, bit (x % 32) of
        // the word at (x / 32) -- the layout the bitmap converters read back. Start
        // from a cleared mask so only the bits set below are opaque.
        if (mdata && (mbpp == 1)) {
            std::memset(mdata, 0, static_cast<std::size_t>(mbw) * h);
        }

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const std::uint8_t r = rgba[(y * w + x) * 4 + 0];
                const std::uint8_t g = rgba[(y * w + x) * 4 + 1];
                const std::uint8_t b = rgba[(y * w + x) * 4 + 2];
                const std::uint8_t a = rgba[(y * w + x) * 4 + 3];

                if (cdata) {
                    if (cbpp == 16) {
                        const std::uint16_t px = static_cast<std::uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                        *reinterpret_cast<std::uint16_t *>(cdata + y * cbw + x * 2) = px;
                    } else if (cbpp == 32) {
                        std::uint8_t *p = cdata + y * cbw + x * 4;
                        p[0] = b; p[1] = g; p[2] = r;
                        p[3] = epoc::is_display_mode_alpha(cmode) ? a : 0xFF;
                    } else if (cbpp == 24) {
                        std::uint8_t *p = cdata + y * cbw + x * 3;
                        p[0] = b; p[1] = g; p[2] = r;
                    }
                }

                if (mdata) {
                    if (mbpp == 1) {
                        if (a >= 0x80) {
                            std::uint32_t *word = reinterpret_cast<std::uint32_t *>(mdata + y * mbw) + (x / 32);
                            *word |= 1u << (x & 0x1F);
                        }
                    } else if (mbpp == 8) {
                        mdata[y * mbw + x] = a;
                    }
                }
            }
        }
    }

    // Load a raster .mbm bitmap entry into RGBA8888 at its natural size.
    static bool load_mbm_icon_rgba(io_system *io, fbs_server *fbss, const std::u16string &path,
        const int index, std::vector<std::uint8_t> &rgba, int &out_w, int &out_h) {
        symfile f = open_icon_container(io, path);
        if (!f) {
            return false;
        }

        ro_file_stream rfs(f.get());
        loader::mbm_file parser(reinterpret_cast<common::ro_stream *>(&rfs));
        if (!parser.do_read_headers()) {
            return false;
        }
        if ((index < 0) || (static_cast<std::size_t>(index) >= parser.sbm_headers.size())) {
            return false;
        }

        out_w = parser.sbm_headers[index].size_pixels.x;
        out_h = parser.sbm_headers[index].size_pixels.y;
        if ((out_w <= 0) || (out_h <= 0)) {
            return false;
        }

        rgba.assign(static_cast<std::size_t>(out_w) * out_h * 4, 0);
        common::wo_buf_stream dst(rgba.data(), rgba.size());
        return epoc::convert_to_rgba8888(fbss, parser, static_cast<std::size_t>(index), dst);
    }

    // Decide the final raster size from the requested one, treating KMaxTInt / <=0 as
    // "derive from the source aspect ratio".
    static void compute_icon_size(const int req_w, const int req_h, const int nat_w, const int nat_h,
        int &out_w, int &out_h) {
        const bool valid_w = (req_w > 0) && (req_w <= 4096);
        const bool valid_h = (req_h > 0) && (req_h <= 4096);
        if (valid_w && valid_h) {
            out_w = req_w;
            out_h = req_h;
        } else if (valid_w) {
            out_w = req_w;
            out_h = (nat_w > 0) ? (req_w * nat_h / nat_w) : nat_h;
        } else if (valid_h) {
            out_h = req_h;
            out_w = (nat_h > 0) ? (req_h * nat_w / nat_h) : nat_w;
        } else {
            out_w = nat_w;
            out_h = nat_h;
        }
        out_w = common::max(1, common::min(out_w, 4096));
        out_h = common::max(1, common::min(out_h, 4096));
    }

    static void scale_rgba_nearest(const std::uint8_t *src, const int sw, const int sh,
        std::uint8_t *dst, const int dw, const int dh) {
        for (int y = 0; y < dh; y++) {
            const int sy = (sh > 0) ? (y * sh / dh) : 0;
            for (int x = 0; x < dw; x++) {
                const int sx = (sw > 0) ? (x * sw / dw) : 0;
                std::memcpy(dst + (static_cast<std::size_t>(y) * dw + x) * 4,
                    src + (static_cast<std::size_t>(sy) * sw + sx) * 4, 4);
            }
        }
    }

    // Produce a rendered RGBA buffer for an icon container entry, honouring the requested
    // size. Handles vector (.mif SVG/NVG via lunasvg) and raster (.mbm) containers.
    static bool produce_icon_rgba(io_system *io, fbs_server *fbss, const std::u16string &path,
        const int index, const int req_w, const int req_h, std::vector<std::uint8_t> &out_rgba,
        int &fw, int &fh) {
        auto sane = [](const int v, const int fallback) -> int {
            return ((v <= 0) || (v > 4096)) ? fallback : v;
        };
        const int prov_w = sane(req_w, sane(req_h, 64));
        const int prov_h = sane(req_h, prov_w);

        // Vector container first.
        std::unique_ptr<lunasvg::Document> doc = load_mif_icon_document(io, path, index, prov_w, prov_h);
        if (doc) {
            const int nw = (doc->width() > 0) ? static_cast<int>(doc->width()) : prov_w;
            const int nh = (doc->height() > 0) ? static_cast<int>(doc->height()) : prov_h;
            compute_icon_size(req_w, req_h, nw, nh, fw, fh);

            out_rgba.assign(static_cast<std::size_t>(fw) * fh * 4, 0);
            const float sx = (doc->width() > 0) ? (static_cast<float>(fw) / doc->width()) : 1.0f;
            const float sy = (doc->height() > 0) ? (static_cast<float>(fh) / doc->height()) : 1.0f;
            lunasvg::Bitmap luna_bmp(out_rgba.data(), fw, fh, fw * 4);
            doc->render(luna_bmp, lunasvg::Matrix{ sx, 0, 0, sy, 0, 0 });
            luna_bmp.convertToRGBA();
            return true;
        }

        // Raster container (.mbm).
        std::vector<std::uint8_t> natural;
        int nw = 0;
        int nh = 0;
        if (load_mbm_icon_rgba(io, fbss, path, index, natural, nw, nh)) {
            compute_icon_size(req_w, req_h, nw, nh, fw, fh);
            if ((fw == nw) && (fh == nh)) {
                out_rgba = std::move(natural);
            } else {
                out_rgba.assign(static_cast<std::size_t>(fw) * fh * 4, 0);
                scale_rgba_nearest(natural.data(), nw, nh, out_rgba.data(), fw, fh);
            }
            return true;
        }

        return false;
    }

    void akn_icon_server::retrieve_icon(service::ipc_context *ctx) {
        std::optional<epoc::akn_icon_params> spec = ctx->get_argument_data_from_descriptor<epoc::akn_icon_params>(0);
        std::optional<epoc::akn_icon_srv_return_data> ret = ctx->get_argument_data_from_descriptor<epoc::akn_icon_srv_return_data>(1);

        if (!spec || !ret) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::size_t icon_index = 0;
        std::optional<epoc::akn_icon_srv_return_data> cached = find_existing_icon(spec.value(), &icon_index);
        if (!cached) {
            io_system *io = sys->get_io_system();
            const std::u16string icon_path = spec->file_name.to_std_string(nullptr);

            std::vector<std::uint8_t> rgba;
            int w = 0;
            int h = 0;
            const bool rendered = produce_icon_rgba(io, fbss, icon_path, static_cast<int>(spec->bitmap_id),
                spec->size.x, spec->size.y, rgba, w, h);

            if (!rendered) {
                // Fall back to a sane requested/default size for the (transparent) placeholder.
                w = ((spec->size.x > 0) && (spec->size.x <= 4096)) ? spec->size.x : 1;
                h = ((spec->size.y > 0) && (spec->size.y <= 4096)) ? spec->size.y : 1;
            }

            fbs_bitmap_data_info info;
            info.size_ = eka2l1::vec2(w, h);
            info.dpm_ = init_data.icon_mode;

            fbsbitmap *bmp = fbss->create_bitmap(info);

            info.dpm_ = init_data.icon_mask_mode;
            fbsbitmap *mask = fbss->create_bitmap(info);

            if (!bmp || !mask) {
                ctx->complete(epoc::error_no_memory);
                return;
            }

            if (rendered) {
                blit_rgba_into_icon(fbss, bmp->bitmap_, mask->bitmap_, rgba.data(), w, h);
            } else {
                // No renderer for this container — make the icon fully transparent so it
                // doesn't show up as a white box.
                if (mask->bitmap_) {
                    std::uint8_t *mdata = mask->bitmap_->data_pointer(fbss);
                    if (mdata) {
                        std::memset(mdata, 0, static_cast<std::size_t>(mask->bitmap_->byte_width_) * h);
                    }
                }
                LOG_TRACE(SERVICE_UI, "AknIconServer: no renderer for icon idx {} in container", spec->bitmap_id);
            }

            // Increase ref
            bmp->ref();
            mask->ref();

            ret->bitmap_handle = bmp->id;
            ret->content_dim.x = w;
            ret->content_dim.y = h;
            ret->mask_handle = mask->id;

            add_icon(ret.value(), spec.value());
        } else {
            ret.emplace(cached.value());
            icons[icon_index].use_count++;
        }

        ctx->write_data_to_descriptor_argument(0, spec.value());
        ctx->write_data_to_descriptor_argument(1, ret.value());
        ctx->complete(epoc::error_none);
    }

    bool akn_icon_server::cache_or_delete_icon(const std::size_t icon_idx) {
        if (icon_idx >= icons.size()) {
            return false;
        }

        icon_data_item &icon = icons[icon_idx];

        // TODO: Cache
        fbsbitmap *original = fbss->get<fbsbitmap>(icon.ret.bitmap_handle);
        fbsbitmap *mask = fbss->get<fbsbitmap>(icon.ret.mask_handle);

        if (original) {
            // Try to free original bitmap, ignore result.
            original->count--;
            fbss->free_bitmap(original);
        }

        if (mask) {
            // Try to free mask bitmap, ignore result
            mask->count--;
            fbss->free_bitmap(mask);
        }

        // Delete the icon from the icon item list
        icons.erase(icons.begin() + icon_idx);

        return true;
    }

    void akn_icon_server::free_bitmap(service::ipc_context *ctx) {
        std::optional<epoc::akn_icon_params> params = ctx->get_argument_data_from_descriptor<epoc::akn_icon_params>(0);

        std::size_t icon_index = 0;
        if (!find_existing_icon(params.value(), &icon_index)) {
            // We can't find the icon. The params is fraud!!
            ctx->complete(epoc::error_not_found);
            return;
        }

        // We have found the icon
        // Decrease the reference count
        icons[icon_index].use_count--;

        // If the reference count is 0, it means no one is using this bitmap anymore, for now. We have two options:
        // Cache the bitmap, or delete it from the server
        if (icons[icon_index].use_count == 0) {
            if (!cache_or_delete_icon(icon_index)) {
                ctx->complete(epoc::error_general);
                return;
            }
        }

        // Success, return error none.
        ctx->complete(epoc::error_none);
    }

    std::optional<epoc::akn_icon_srv_return_data> akn_icon_server::find_existing_icon(epoc::akn_icon_params &spec, std::size_t *idx) {
        for (std::size_t i = 0; i < icons.size(); i++) {
            epoc::akn_icon_params cached_spec = icons[i].spec;

            /**
             * The original implementation only check for the equal of:
             * - The bitmap ID.
             * - The bitmap container file (compare folded aka ignoring case)
             * - App icon?
             *
             * These three are the decesive elements that decide if two requests are trying to get the same bitmap.
             */
            if (spec.bitmap_id == cached_spec.bitmap_id) {
                if (common::compare_ignore_case(spec.file_name.to_std_string(nullptr), cached_spec.file_name.to_std_string(nullptr)) == 0) {
                    if (spec.app_icon == cached_spec.app_icon) {
                        if (idx) {
                            *idx = i;
                        }

                        return icons[i].ret;
                    }
                }
            }
        }
        return std::nullopt;
    }

    void akn_icon_server::add_icon(const epoc::akn_icon_srv_return_data &ret, const epoc::akn_icon_params &spec) {
        icon_data_item item;
        item.ret = ret;
        item.spec = spec;
        item.use_count = 1;

        icons.push_back(item);
    }
}
