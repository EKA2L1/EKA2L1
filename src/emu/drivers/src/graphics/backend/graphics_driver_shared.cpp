/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <common/log.h>
#include <drivers/graphics/backend/graphics_driver_shared.h>

// Use for render text and draw custom bitmap
#include <imgui.h>
#include <imgui_internal.h>

#include <glad/glad.h>

namespace eka2l1::drivers {
    static void translate_bpp_to_format(const int bpp, texture_format &internal_format, texture_format &format) {
        // Hope the driver likes this, it always does.
        internal_format = texture_format::bgra;

        switch (bpp) {
        case 8:
            format = texture_format::r;
            break;

        case 16:
            format = texture_format::rg;
            break;

        case 24:
            format = texture_format::rgb;
            break;

        case 32:
            format = texture_format::rgba;
            break;

        default:
            break;
        }
    }

    bitmap::bitmap(graphics_driver *driver, const eka2l1::vec2 &size, const int initial_bpp)
        : bpp(initial_bpp) {
        // Make color buffer for our bitmap!
        tex = make_texture(driver);

        texture_format internal_format = texture_format::none;
        texture_format data_format = texture_format::none;

        translate_bpp_to_format(bpp, internal_format, data_format);

        tex->create(driver, 2, 0, eka2l1::vec3(size.x, size.y, 0), internal_format, data_format, texture_data_type::ubyte, nullptr);
        tex->set_filter_minmag(false, drivers::filter_option::linear);
        tex->set_filter_minmag(true, drivers::filter_option::linear);
    }

    shared_graphics_driver::shared_graphics_driver(const graphic_api gr_api)
        : graphics_driver(gr_api)
        , binding(nullptr) {
    }

    shared_graphics_driver::~shared_graphics_driver() {
    }

    bitmap *shared_graphics_driver::get_bitmap(const drivers::handle h) {
        if (h > bmp_textures.size()) {
            return nullptr;
        }

        return bmp_textures[h - 1].get();
    }

    void shared_graphics_driver::update_bitmap(drivers::handle h, const std::size_t size, const eka2l1::vec2 &offset,
        const eka2l1::vec2 &dim, int bpp, const void *data) {
        // Get our bitmap
        bitmap *bmp = get_bitmap(h);

        if (bmp == nullptr) {
            return;
        }

        texture_format internal_format = texture_format::none;
        texture_format data_format = texture_format::none;

        translate_bpp_to_format(bpp, internal_format, data_format);

        bmp->tex->update_data(this, 0, eka2l1::vec3(offset.x, offset.y, 0), eka2l1::vec3(dim.x, dim.y, 0), data_format,
            texture_data_type::ubyte, data);
    }

    void shared_graphics_driver::update_bitmap(command_helper &helper) {
        drivers::handle handle = 0;
        void *data = nullptr;
        int bpp = 0;
        std::size_t size = 0;
        eka2l1::vec2 offset;
        eka2l1::vec2 dim;

        helper.pop(handle);
        helper.pop(data);
        helper.pop(bpp);
        helper.pop(size);
        helper.pop(offset);
        helper.pop(dim);

        update_bitmap(handle, size, offset, dim, bpp, data);

        delete data;
    }

    void shared_graphics_driver::create_bitmap(command_helper &helper) {
        eka2l1::vec2 size;
        drivers::handle *result = nullptr;

        helper.pop(size);
        helper.pop(result);

        // Find free slot
        auto slot_free = std::find(bmp_textures.begin(), bmp_textures.end(), nullptr);

        if (slot_free == bmp_textures.end()) {
            *slot_free = std::make_unique<bitmap>(size, 32);
            *result = std::distance(bmp_textures.begin(), slot_free) + 1;
        } else {
            bmp_textures.push_back(std::make_unique<bitmap>(size, 32));
            *result = bmp_textures.size();
        }

        // Notify
        helper.finish(this, 0);
    }

    void shared_graphics_driver::bind_bitmap(command_helper &helper) {
        drivers::handle h = 0;
        helper.pop(h);

        bitmap *bmp = get_bitmap(h);

        if (!bmp) {
            LOG_ERROR("Bitmap handle invalid to be binded");
            return;
        }

        if (!bmp->fb) {
            // Make new one
            bmp->fb = make_framebuffer(this, bmp->tex, nullptr);
        }

        // Bind the framebuffer
        bmp->fb->bind(this);
        binding = bmp;

        // Build projection matrixx
        projection_matrix = glm::ortho(0.0f, static_cast<float>(bmp->tex->get_size().x), static_cast<float>(bmp->tex->get_size().y),
            0.0f, -1.0f, 1.0f);
    }

    void shared_graphics_driver::destroy_bitmap(command_helper &helper) {
        drivers::handle h = 0;
        helper.pop(h);

        if (h > bmp_textures.size()) {
            LOG_ERROR("Invalid bitmap handle to destroy");
            return;
        }

        bmp_textures[h - 1].reset();
    }

    void shared_graphics_driver::dispatch(command *cmd) {
        command_helper helper(cmd);

        switch (cmd->opcode_) {
        case graphics_driver_create_bitmap: {
            create_bitmap(helper);
            break;
        }

        case graphics_driver_bind_bitmap: {
            bind_bitmap(helper);
            break;
        }

        case graphics_driver_update_bitmap: {
            update_bitmap(helper);
            break;
        }

        case graphics_driver_destroy_bitmap: {
            destroy_bitmap(helper);
            break;
        }

        default:
            break;
        }
    }
}