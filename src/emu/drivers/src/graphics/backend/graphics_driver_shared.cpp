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
#include <drivers/graphics/buffer.h>
#include <drivers/graphics/shader.h>

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
        , binding(nullptr)
        , brush_color({ 1.0f, 1.0f, 1.0f, 1.0f })
        , current_fb_height(0) {
    }

    shared_graphics_driver::~shared_graphics_driver() {
    }

    bitmap *shared_graphics_driver::get_bitmap(const drivers::handle h) {
        if (h > bmp_textures.size()) {
            return nullptr;
        }

        return bmp_textures[h - 1].get();
    }

    drivers::handle shared_graphics_driver::append_graphics_object(graphics_object_instance &instance) {
        auto free_slot = std::find(graphic_objects.begin(), graphic_objects.end(), nullptr);

        if (free_slot != graphic_objects.end()) {
            *free_slot = std::move(instance);
            return std::distance(graphic_objects.begin(), free_slot) + 1;
        }

        graphic_objects.push_back(std::move(instance));
        return graphic_objects.size();
    }

    bool shared_graphics_driver::delete_graphics_object(const drivers::handle handle) {
        if (handle > graphic_objects.size() || handle == 0) {
            return 0;
        }

        graphic_objects[handle - 1].reset();
        return true;
    }

    graphics_object *shared_graphics_driver::get_graphics_object(const drivers::handle num) {
        if (num > graphic_objects.size() || num == 0) {
            return nullptr;
        }

        return graphic_objects[num - 1].get();
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
            *slot_free = std::make_unique<bitmap>(this, size, 32);
            *result = std::distance(bmp_textures.begin(), slot_free) + 1;
        } else {
            bmp_textures.push_back(std::make_unique<bitmap>(this, size, 32));
            *result = bmp_textures.size();
        }

        // Notify
        helper.finish(this, 0);
    }

    void shared_graphics_driver::bind_bitmap(command_helper &helper) {
        drivers::handle h = 0;
        helper.pop(h);

        if (h == 0) {
            current_fb_height = swapchain_size.y;
            binding = nullptr;

            return;
        }

        bitmap *bmp = get_bitmap(h);

        if (!bmp) {
            LOG_ERROR("Bitmap handle invalid to be binded");
            return;
        }

        if (!bmp->fb) {
            // Make new one
            bmp->fb = make_framebuffer(this, bmp->tex.get(), nullptr);
        }

        // Bind the framebuffer
        bmp->fb->bind(this);
        binding = bmp;

        // Build projection matrixx
        projection_matrix = glm::ortho(0.0f, static_cast<float>(bmp->tex->get_size().x), static_cast<float>(bmp->tex->get_size().y),
            0.0f, -1.0f, 1.0f);

        current_fb_height = bmp->tex->get_size().y;
        set_viewport(eka2l1::rect(eka2l1::vec2(0, 0), bmp->tex->get_size()));
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

    void shared_graphics_driver::resize_bitmap(command_helper &helper) {
        drivers::handle h = 0;
        helper.pop(h);

        bitmap *bmp = get_bitmap(h);

        if (!bmp) {
            LOG_ERROR("Bitmap handle invalid to be binded");
            return;
        }

        vec2 new_size = { 0, 0 };
        helper.pop(new_size);

        // Change texture size
        bmp->tex->change_size({ new_size.x, new_size.y, 0 });
        bmp->tex->tex(this, false);
    }

    void shared_graphics_driver::set_brush_color(command_helper &helper) {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;

        helper.pop(r);
        helper.pop(g);
        helper.pop(b);
        helper.pop(a);

        brush_color = { r, g, b, a };
    }

    void shared_graphics_driver::create_program(command_helper &helper) {
        char *vert_data = nullptr;
        char *frag_data = nullptr;
        void **metadata = nullptr;

        std::size_t vert_size = 0;
        std::size_t frag_size = 0;

        helper.pop(vert_data);
        helper.pop(frag_data);
        helper.pop(vert_size);
        helper.pop(frag_size);
        helper.pop(metadata);

        auto obj = make_shader(this);
        if (!obj->create(this, vert_data, vert_size, frag_data, frag_size)) {
            LOG_ERROR("Fail to create shader");
            return;
        }

        if (metadata) {
            *metadata = obj->get_metadata();
        }

        std::unique_ptr<graphics_object> obj_casted = std::move(obj);
        drivers::handle res = append_graphics_object(obj_casted);

        drivers::handle *store = nullptr;
        helper.pop(store);

        *store = res;
        helper.finish(this, 0);
    }

    void shared_graphics_driver::create_texture(command_helper &helper) {
        std::uint8_t dim = 0;
        std::uint8_t mip_level = 0;

        drivers::texture_format internal_format = drivers::texture_format::none;
        drivers::texture_format data_format = drivers::texture_format::none;
        drivers::texture_data_type data_type = drivers::texture_data_type::ubyte;
        void *data = nullptr;

        helper.pop(dim);
        helper.pop(mip_level);
        helper.pop(internal_format);
        helper.pop(data_format);
        helper.pop(data_type);
        helper.pop(data);

        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t depth = 0;

        if (dim >= 1) {
            helper.pop(width);

            if (dim >= 2) {
                helper.pop(height);

                if (dim == 3) {
                    helper.pop(depth);
                }
            }
        }

        auto obj = make_texture(this);
        obj->create(this, static_cast<int>(dim), static_cast<int>(mip_level), eka2l1::vec3(width, height, depth),
            internal_format, data_format, data_type, data);

        std::unique_ptr<graphics_object> obj_casted = std::move(obj);
        drivers::handle res = append_graphics_object(obj_casted);

        drivers::handle *store = nullptr;
        helper.pop(store);

        *store = res;

        helper.finish(this, 0);
    }

    void shared_graphics_driver::create_buffer(command_helper &helper) {
        std::size_t initial_size = 0;
        buffer_hint hint = buffer_hint::none;
        buffer_upload_hint upload_hint = static_cast<buffer_upload_hint>(0);

        helper.pop(initial_size);
        helper.pop(hint);
        helper.pop(upload_hint);

        auto obj = make_buffer(this);
        obj->create(this, initial_size, hint, upload_hint);

        std::unique_ptr<graphics_object> obj_casted = std::move(obj);
        drivers::handle res = append_graphics_object(obj_casted);

        drivers::handle *store = nullptr;
        helper.pop(store);

        *store = res;

        helper.finish(this, 0);
    }

    void shared_graphics_driver::use_program(command_helper &helper) {
        drivers::handle num;
        helper.pop(num);

        shader *shobj = reinterpret_cast<shader *>(get_graphics_object(num));

        if (!shobj) {
            return;
        }

        shobj->use(this);
    }

    void shared_graphics_driver::set_uniform(command_helper &helper) {
        drivers::handle num;
        drivers::shader_set_var_type var_type;
        void *data = nullptr;
        int binding = 0;

        helper.pop(num);
        helper.pop(var_type);
        helper.pop(data);
        helper.pop(binding);

        shader *shobj = reinterpret_cast<shader *>(get_graphics_object(num));

        if (!shobj) {
            return;
        }

        shobj->set(this, binding, var_type, data);

        delete data;
    }

    void shared_graphics_driver::bind_texture(command_helper &helper) {
        drivers::handle num = 0;
        int binding = 0;

        helper.pop(num);
        helper.pop(binding);

        texture *texobj = reinterpret_cast<texture *>(get_graphics_object(num));

        if (!texobj) {
            return;
        }

        texobj->bind(this, binding);
    }

    void shared_graphics_driver::bind_buffer(command_helper &helper) {
        drivers::handle num = 0;

        helper.pop(num);

        buffer *bufobj = reinterpret_cast<buffer *>(get_graphics_object(num));

        if (!bufobj) {
            return;
        }

        bufobj->bind(this);
    }

    void shared_graphics_driver::update_buffer(command_helper &helper) {
        drivers::handle num = 0;
        void *data = nullptr;
        std::size_t offset = 0;
        std::size_t size = 0;

        helper.pop(num);
        helper.pop(data);
        helper.pop(offset);
        helper.pop(size);

        buffer *bufobj = reinterpret_cast<buffer *>(get_graphics_object(num));

        if (!bufobj) {
            return;
        }

        bufobj->update_data(this, data, offset, size);

        delete data;
    }

    void shared_graphics_driver::attach_descriptors(drivers::handle h, const int stride, const bool instance_move,
        const attribute_descriptor *descriptors, const int descriptor_count) {
        buffer *bufobj = reinterpret_cast<buffer *>(get_graphics_object(h));

        if (!bufobj) {
            return;
        }

        bufobj->attach_descriptors(this, stride, instance_move, descriptors, descriptor_count);
    }

    void shared_graphics_driver::attach_descriptors(command_helper &helper) {
        drivers::handle h = 0;
        int stride = 0;
        bool instance_move = false;
        attribute_descriptor *descriptors = nullptr;
        int descriptor_count = 0;

        helper.pop(h);
        helper.pop(stride);
        helper.pop(instance_move);
        helper.pop(descriptors);
        helper.pop(descriptor_count);

        attach_descriptors(h, stride, instance_move, descriptors, descriptor_count);
    }

    void shared_graphics_driver::destroy_object(command_helper &helper) {
        drivers::handle h = 0;
        helper.pop(h);

        delete_graphics_object(h);
    }

    void shared_graphics_driver::set_filter(command_helper &helper) {
        drivers::handle h = 0;
        drivers::filter_option min = drivers::filter_option::linear;
        drivers::filter_option mag = drivers::filter_option::linear;

        helper.pop(h);
        helper.pop(min);
        helper.pop(mag);

        texture *texobj = reinterpret_cast<texture *>(get_graphics_object(h));

        if (!texobj) {
            return;
        }

        texobj->set_filter_minmag(false, min);
        texobj->set_filter_minmag(true, mag);
    }

    void shared_graphics_driver::set_swapchain_size(command_helper &helper) {
        eka2l1::vec2 size;
        helper.pop(size);

        swapchain_size = size;

        if (!binding) {
            current_fb_height = swapchain_size.y;
        }
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

        case graphics_driver_destroy_object: {
            destroy_object(helper);
            break;
        }

        case graphics_driver_set_brush_color: {
            set_brush_color(helper);
            break;
        }

        case graphics_driver_create_program: {
            create_program(helper);
            break;
        }

        case graphics_driver_create_texture: {
            create_texture(helper);
            break;
        }

        case graphics_driver_create_buffer: {
            create_buffer(helper);
            break;
        }

        case graphics_driver_set_uniform: {
            set_uniform(helper);
            break;
        }

        case graphics_driver_use_program: {
            use_program(helper);
            break;
        }

        case graphics_driver_bind_texture: {
            bind_texture(helper);
            break;
        }

        case graphics_driver_bind_buffer: {
            bind_buffer(helper);
            break;
        }

        case graphics_driver_update_buffer: {
            update_buffer(helper);
            break;
        }

        case graphics_driver_attach_descriptors: {
            attach_descriptors(helper);
            break;
        }

        case graphics_driver_set_texture_filter: {
            set_filter(helper);
            break;
        }

        case graphics_driver_set_swapchain_size: {
            set_swapchain_size(helper);
            break;
        }

        default:
            LOG_ERROR("Unimplemented opcode {} for graphics driver", cmd->opcode_);
            break;
        }
    }
}