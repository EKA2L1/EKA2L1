/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include "context.h"

#include <control/dispatcher.h>
#include <control/new_file.h>
#include <control/params.h>

#include <common/crypt.h>
#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>
#include <kernel/kernel.h>
#include <services/window/screen.h>
#include <services/window/window.h>
#include <system/epoc.h>

#include <mutex>
#include <string>
#include <vector>

// Private to this file: the Android frontend compiles its own copy.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace eka2l1::control {
    namespace {
        struct screen_snapshot {
            drivers::graphics_driver *driver = nullptr;
            drivers::handle texture = 0;
            eka2l1::vec2 size;
        };

        std::string encode_png(const std::vector<std::uint8_t> &rgba, const eka2l1::vec2 &size) {
            // The guest leaves alpha undefined in places; a screenshot is opaque.
            std::vector<std::uint8_t> rgb(static_cast<std::size_t>(size.x) * size.y * 3);

            for (std::size_t pixel = 0; pixel < rgb.size() / 3; pixel++) {
                rgb[pixel * 3 + 0] = rgba[pixel * 4 + 0];
                rgb[pixel * 3 + 1] = rgba[pixel * 4 + 1];
                rgb[pixel * 3 + 2] = rgba[pixel * 4 + 2];
            }

            std::string png;
            const auto append = [](void *context, void *data, int size) {
                reinterpret_cast<std::string *>(context)->append(reinterpret_cast<const char *>(data), static_cast<std::size_t>(size));
            };

            if (stbi_write_png_to_func(append, &png, size.x, size.y, 3, rgb.data(), size.x * 3) == 0) {
                throw rpc_error(error_internal, "Encoding the screenshot as PNG failed");
            }

            return png;
        }
    }

    void add_screen_methods(dispatcher &rpc, context &ctx) {
        rpc.add("screen.capture", [&ctx](session &, const rapidjson::Value &params, rapidjson::Value &result, json_allocator &allocator) {
            const std::optional<std::string> path = optional_string(params, "path");
            const std::optional<std::int64_t> screen_number = optional_integer(params, "screen", 0, 255);

            screen_snapshot snapshot;

            ctx.run_in_guest([&](system &sys) {
                kernel_system &kern = require_kernel(sys);
                window_server &winserv = require_window_server(sys);

                snapshot.driver = sys.get_graphics_driver();

                if (!snapshot.driver) {
                    throw rpc_error(error_not_ready, "The emulator has no graphics driver yet");
                }

                epoc::screen *scr = screen_number ? winserv.get_screen(static_cast<int>(*screen_number)) : winserv.get_current_focus_screen();

                if (!scr) {
                    throw rpc_error(error_not_found, "There is no screen " + std::to_string(screen_number.value_or(0)));
                }

                // The redraw on the timer thread holds both, in this order, while it draws.
                const std::lock_guard<kernel_system> kernel_guard(kern);
                const std::lock_guard<std::mutex> screen_guard(scr->screen_mutex);

                snapshot.texture = scr->screen_texture;
                snapshot.size = scr->current_mode().size * scr->display_scale_factor;
            });

            if (!snapshot.texture || (snapshot.size.x <= 0) || (snapshot.size.y <= 0)) {
                throw rpc_error(error_failed, "The screen has not been drawn yet");
            }

            // A synchronous command to the graphics thread, sent with no emulator lock held: the
            // commands drawing the screen were submitted before it, so they are read back done.
            std::vector<std::uint8_t> rgba(static_cast<std::size_t>(snapshot.size.x) * snapshot.size.y * 4);

            if (!drivers::read_bitmap(snapshot.driver, snapshot.texture, eka2l1::point(0, 0), snapshot.size, 32, rgba.data())) {
                throw rpc_error(error_failed, "Reading the screen back from the graphics driver failed");
            }

            const std::string png = encode_png(rgba, snapshot.size);

            result.AddMember("width", snapshot.size.x, allocator);
            result.AddMember("height", snapshot.size.y, allocator);

            if (path) {
                write_new_file(*path, png);
                result.AddMember("path", rapidjson::Value(path->c_str(), allocator), allocator);
            } else {
                const std::string encoded = crypt::base64_encode(reinterpret_cast<const std::uint8_t *>(png.data()), png.size());
                result.AddMember("png", rapidjson::Value(encoded.c_str(), static_cast<rapidjson::SizeType>(encoded.size()), allocator), allocator);
            }
        });
    }
}
