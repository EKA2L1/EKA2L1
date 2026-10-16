// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <services/applist/applist.h>
#include <services/fbs/bitmap.h>
#include <common/buffer.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <loader/mbm.h>
#include <loader/mif.h>
#include <system/devices.h>
#include <system/epoc.h>

#include <cmath>
#include <limits>
#include <lunasvg.h>

namespace eka2l1 {
    static std::optional<app_icon_image> allocate_icon(const vec2 &size) {
        if (size.x <= 0 || size.y <= 0 || size.x > std::numeric_limits<std::int32_t>::max() / 4
            || static_cast<std::size_t>(size.x) > std::numeric_limits<std::size_t>::max() / size.y / 4) {
            return std::nullopt;
        }
        return app_icon_image{ size, std::vector<std::uint8_t>(static_cast<std::size_t>(size.x) * size.y * 4) };
    }

    static void premultiply_icon(app_icon_image &icon) {
        for (std::size_t i = 0; i < icon.pixels.size(); i += 4) {
            const std::uint32_t alpha = icon.pixels[i + 3];
            for (std::size_t channel = 0; channel < 3; ++channel) {
                icon.pixels[i + channel] = (icon.pixels[i + channel] * alpha + 127) / 255;
            }
        }
    }

    std::optional<app_icon_image> decode_mbm_app_icon(fbs_server *fbs, common::ro_stream &stream, const epocver version) {
        loader::mbm_file parser(&stream);
        if (!parser.do_read_headers() || parser.sbm_headers.empty()) {
            return std::nullopt;
        }
        auto icon = allocate_icon(parser.sbm_headers[0].size_pixels);
        if (!icon) return std::nullopt;
        common::wo_buf_stream output(icon->pixels.data(), icon->pixels.size());
        if (!epoc::convert_to_rgba8888(fbs, parser, 0, output)) return std::nullopt;

        if (parser.sbm_headers.size() > 1 && parser.sbm_headers[1].size_pixels == icon->size) {
            std::vector<std::uint8_t> mask(icon->pixels.size());
            common::wo_buf_stream mask_output(mask.data(), mask.size());
            if (epoc::convert_to_rgba8888(fbs, parser, 1, mask_output, true)) {
                const auto &header = parser.sbm_headers[1];
                const auto composition = version >= epocver::epoc10
                    ? epoc::icon_mask_composition::alf_canvas : epoc::icon_mask_composition::avkon;
                epoc::apply_icon_mask_alpha(icon->pixels.data(), mask.data(), icon->size.x, icon->size.y,
                    epoc::get_display_mode_from_bpp(header.bit_per_pixels, header.color), composition);
            }
        }
        premultiply_icon(*icon);
        return icon;
    }

    static std::optional<app_icon_image> decode_mif_app_icon(io_system *io, const apa_app_registry &registry,
        const std::string &cache_dir) {
        auto file = io->open_file(registry.icon_file_path, READ_MODE | BIN_MODE);
        if (!file) return std::nullopt;
        const std::string cached_path = cache_dir.empty() ? std::string()
            : add_path(cache_dir, fmt::format("debinarized_{:08X}.svg", registry.mandatory_info.uid));
        std::unique_ptr<lunasvg::Document> document;
        if (!cached_path.empty() && common::exists(cached_path)
            && common::get_last_modifiy_since_ad(common::utf8_to_ucs2(cached_path)) >= file->last_modify_since_0ad()) {
            document = lunasvg::Document::loadFromFile(cached_path);
        }
        if (!document) {
            ro_file_stream stream(file.get());
            loader::mif_file parser(&stream);
            int size = 0;
            if (parser.do_parse() && parser.read_mif_entry(0, nullptr, size) && size > 0) {
                std::vector<std::uint8_t> data(size);
                if (parser.read_mif_entry(0, data.data(), size)) {
                    common::wo_growable_buf_stream svg;
                    if (loader::convert_mif_icon_to_svg(data.data(), data.size(), svg)) {
                        const std::string contents = svg.content();
                        document = lunasvg::Document::loadFromData(contents);
                        if (document && !cached_path.empty()) {
                            common::create_directories(cache_dir);
                            bool written = false;
                            {
                                common::wo_std_file_stream cached(cached_path, true);
                                written = cached.valid() && cached.write(contents.data(), contents.size()) == contents.size();
                            }
                            if (!written) common::remove(cached_path);
                        }
                    }
                }
            }
            if (!document && !cached_path.empty()) common::remove(cached_path);
        }
        if (!document || !std::isfinite(document->width()) || !std::isfinite(document->height())
            || document->width() <= 0 || document->height() <= 0
            || static_cast<double>(document->width()) > std::numeric_limits<std::int32_t>::max()
            || static_cast<double>(document->height()) > std::numeric_limits<std::int32_t>::max()) {
            return std::nullopt;
        }
        auto icon = allocate_icon(vec2(document->width(), document->height()));
        if (!icon) return std::nullopt;
        lunasvg::Bitmap bitmap(icon->pixels.data(), icon->size.x, icon->size.y, icon->size.x * 4);
        document->render(bitmap, lunasvg::Matrix{ 1, 0, 0, 1, 0, 0 });
        bitmap.convertToRGBA();
        premultiply_icon(*icon);
        return icon;
    }

    std::optional<app_icon_image> applist_server::get_host_icon(apa_app_registry &registry, const std::string &cache_root) {
        auto *io = sys->get_io_system();
        const auto extension = common::lowercase_ucs2_string(path_extension(registry.icon_file_path));
        if (extension == u".mif") {
            std::string cache_dir = cache_root;
            if (!cache_dir.empty()) {
                if (auto *device = sys->get_device_manager()->get_current()) {
                    cache_dir = add_path(cache_dir, common::lowercase_string(device->firmware_code));
                }
            }
            if (auto icon = decode_mif_app_icon(io, registry, cache_dir)) return icon;
        } else if (extension == u".mbm") {
            auto file = io->open_file(registry.icon_file_path, READ_MODE | BIN_MODE);
            if (file) {
                ro_file_stream stream(file.get());
                if (auto icon = decode_mbm_app_icon(fbsserv, stream, sys->get_symbian_version_use())) return icon;
            }
        }

        auto pair = get_list_icon(registry);
        if (!pair || !pair->first) return std::nullopt;
        auto icon = allocate_icon(pair->first->header_.size_pixels);
        if (!icon) return std::nullopt;
        common::wo_buf_stream output(icon->pixels.data(), icon->pixels.size());
        if (!epoc::convert_to_rgba8888(fbsserv, pair->first, output)) return std::nullopt;
        if (pair->second && pair->second->header_.size_pixels == icon->size) {
            std::vector<std::uint8_t> mask(icon->pixels.size());
            common::wo_buf_stream mask_output(mask.data(), mask.size());
            if (epoc::convert_to_rgba8888(fbsserv, pair->second, mask_output, true)) {
                epoc::apply_icon_mask_alpha(icon->pixels.data(), mask.data(), icon->size.x, icon->size.y,
                    pair->second->current_display_mode());
            }
        }
        premultiply_icon(*icon);
        return icon;
    }
}
