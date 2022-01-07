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

#include <common/algorithm.h>
#include <common/log.h>
#include <common/platform.h>

#include <drivers/graphics/backend/graphics_driver_shared.h>
#include <drivers/graphics/buffer.h>
#include <drivers/graphics/shader.h>

#include <glad/glad.h>

namespace eka2l1::drivers {
    static void translate_bpp_to_format(const int bpp, texture_format &internal_format, texture_format &format,
        texture_data_type &data_type, const bool stricted) {
        // Hope the driver likes this, it always does.
        internal_format = texture_format::rgba;
        data_type = texture_data_type::ubyte;

        switch (bpp) {
        case 8:
            format = texture_format::r;

            if (stricted)
                internal_format = texture_format::r8;

            break;

        case 12:
            format = texture_format::rgba;
            internal_format = texture_format::rgba4;
            data_type = texture_data_type::ushort_4_4_4_4;

            break;

        case 16:
            format = texture_format::rgb;
            data_type = texture_data_type::ushort_5_6_5;

            if (stricted)
                internal_format = texture_format::rgb;

            break;

        case 24:
            if (stricted) {
                format = texture_format::rgb;
                internal_format = texture_format::rgb;
            } else {
                format = texture_format::bgr;
            }

            break;

        case 32:
            format = texture_format::bgra;

            if (stricted)
                internal_format = texture_format::bgra;

            break;

        default:
            break;
        }
    }

    static texture_ptr instantiate_suit_color_bitmap_texture(graphics_driver *driver, const eka2l1::vec2 &size, const int bpp) {
        auto texture = make_texture(driver);

        texture_format internal_format = texture_format::none;
        texture_format data_format = texture_format::none;
        texture_data_type data_type = texture_data_type::ubyte;

        translate_bpp_to_format(bpp, internal_format, data_format, data_type, driver->is_stricted());

        texture->create(driver, 2, 0, eka2l1::vec3(size.x, size.y, 0), internal_format, data_format, data_type, nullptr);
        texture->set_filter_minmag(false, drivers::filter_option::linear);
        texture->set_filter_minmag(true, drivers::filter_option::linear);

        if (bpp == 12) {
            texture->set_channel_swizzle({ channel_swizzle::green, channel_swizzle::blue,
                channel_swizzle::alpha, channel_swizzle::one });
        }

        return texture;
    }

    static texture_ptr instantiate_bitmap_depth_stencil_texture(graphics_driver *driver, const eka2l1::vec2 &size) {
        auto ds_tex = make_texture(driver);
        ds_tex->create(driver, 2, 0, eka2l1::vec3(size.x, size.y, 0),
            texture_format::depth24_stencil8, texture_format::depth_stencil, texture_data_type::uint_24_8,
            nullptr, 0);

        return ds_tex;
    }

    bitmap::bitmap(graphics_driver *driver, const eka2l1::vec2 &size, const int initial_bpp)
        : bpp(initial_bpp) {
        // Make color buffer for our bitmap!
        tex = std::move(instantiate_suit_color_bitmap_texture(driver, size, bpp));
    }

    bitmap::~bitmap() {
        tex.reset();
        fb.reset();
    }

    void bitmap::resize(graphics_driver *driver, const eka2l1::vec2 &new_size) {
        auto tex_to_replace = std::move(instantiate_suit_color_bitmap_texture(driver, new_size, bpp));
        auto ds_tex_to_replace = std::move(instantiate_bitmap_depth_stencil_texture(driver, new_size));

        if (fb) {
            auto new_fb = make_framebuffer(driver, { tex_to_replace.get() }, ds_tex_to_replace.get());

            fb->bind(driver, framebuffer_bind_read);
            new_fb->bind(driver, framebuffer_bind_draw);

            eka2l1::rect copy_region;
            copy_region.top = { 0, 0 };
            copy_region.size = eka2l1::object_size(common::min<int>(tex->get_size().x, new_size.x), common::min<int>(tex->get_size().y, new_size.y));

            fb->blit(copy_region, copy_region, draw_buffer_bit_color_buffer, filter_option::linear);

            new_fb->unbind(driver);
            fb->unbind(driver);

            fb = std::move(new_fb);
        }

        tex = std::move(tex_to_replace);
        ds_tex = std::move(ds_tex_to_replace);
    }

    void bitmap::init_fb(graphics_driver *driver) {
        ds_tex = std::move(instantiate_bitmap_depth_stencil_texture(driver, tex->get_size()));
        fb = make_framebuffer(driver, { tex.get() }, ds_tex.get());
    }

    shared_graphics_driver::shared_graphics_driver(const graphic_api gr_api)
        : graphics_driver(gr_api)
        , binding(nullptr)
        , brush_color({ 255.0f, 255.0f, 255.0f, 255.0f })
        , current_fb_height(0) {
    }

    shared_graphics_driver::~shared_graphics_driver() {
    }

#define HANDLE_BITMAP (1ULL << 32)

    bitmap *shared_graphics_driver::get_bitmap(const drivers::handle h) {
        if ((h & HANDLE_BITMAP) == 0) {
            return nullptr;
        }

        if ((h & ~HANDLE_BITMAP) > bmp_textures.size()) {
            return nullptr;
        }

        return bmp_textures[(h & ~HANDLE_BITMAP) - 1].get();
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
        const eka2l1::vec2 &dim, const void *data, const std::size_t pixels_per_line) {
        // Get our bitmap
        bitmap *bmp = get_bitmap(h);

        if (bmp == nullptr) {
            return;
        }

        texture_format internal_format = texture_format::none;
        texture_format data_format = texture_format::none;
        texture_data_type data_type = texture_data_type::ubyte;

        translate_bpp_to_format(bmp->bpp, internal_format, data_format, data_type, is_stricted());

        bmp->tex->update_data(this, 0, eka2l1::vec3(offset.x, offset.y, 0), eka2l1::vec3(dim.x, dim.y, 0), pixels_per_line,
            data_format, data_type, data);

        if (bmp->bpp == 12) {
            bmp->tex->set_channel_swizzle({ channel_swizzle::green, channel_swizzle::blue,
                channel_swizzle::alpha, channel_swizzle::one });
        }

        if (is_stricted()) {
            switch (bmp->bpp) {
            case 8:
                bmp->tex->set_channel_swizzle({ channel_swizzle::red, channel_swizzle::red,
                    channel_swizzle::red, channel_swizzle::red });
                break;

            case 16:
                bmp->tex->set_channel_swizzle({ channel_swizzle::red, channel_swizzle::green,
                    channel_swizzle::blue, channel_swizzle::one });
                break;

            case 24:
                bmp->tex->set_channel_swizzle({ channel_swizzle::blue, channel_swizzle::green,
                    channel_swizzle::red, channel_swizzle::one });
                break;

            default:
                break;
            }
        } else {
            switch (data_format) {
            case texture_format::r:
                bmp->tex->set_channel_swizzle({ channel_swizzle::red, channel_swizzle::red,
                    channel_swizzle::red, channel_swizzle::red });
                break;

            case texture_format::rgb:
                bmp->tex->set_channel_swizzle({ channel_swizzle::red, channel_swizzle::green,
                    channel_swizzle::blue, channel_swizzle::one });
                break;

            default:
                break;
            }
        }
    }

    void shared_graphics_driver::update_bitmap(command_helper &helper) {
        drivers::handle handle = 0;
        std::uint8_t *data = nullptr;
        std::size_t size = 0;
        eka2l1::vec2 offset;
        eka2l1::vec2 dim;
        std::size_t pixels_per_line = 0;

        helper.pop(handle);
        helper.pop(data);
        helper.pop(size);
        helper.pop(offset);
        helper.pop(dim);
        helper.pop(pixels_per_line);

        update_bitmap(handle, size, offset, dim, data, pixels_per_line);

        delete data;
    }

    void shared_graphics_driver::update_texture(command_helper &helper) {
        drivers::handle handle = 0;
        std::uint8_t *data = nullptr;
        std::size_t size = 0;
        drivers::texture_format data_format;
        drivers::texture_data_type data_type;
        eka2l1::vec3 offset;
        eka2l1::vec3 dim;
        std::size_t pixels_per_line = 0;

        helper.pop(handle);
        helper.pop(data);
        helper.pop(size);
        helper.pop(data_format);
        helper.pop(data_type);
        helper.pop(offset);
        helper.pop(dim);
        helper.pop(pixels_per_line);

        drivers::texture *obj = reinterpret_cast<drivers::texture*>(get_graphics_object(handle));
        if (!obj) {
            return;
        }

        obj->update_data(this, 0, offset, dim, pixels_per_line, data_format, data_type, data);

        delete data;
    }

    void shared_graphics_driver::create_bitmap(command_helper &helper) {
        eka2l1::vec2 size;
        std::uint32_t bpp = 0;
        drivers::handle *result = nullptr;

        helper.pop(size);
        helper.pop(bpp);
        helper.pop(result);

        // Find free slot
        auto slot_free = std::find(bmp_textures.begin(), bmp_textures.end(), nullptr);

        if (slot_free != bmp_textures.end()) {
            *slot_free = std::make_unique<bitmap>(this, size, static_cast<int>(bpp));
            *result = std::distance(bmp_textures.begin(), slot_free) + 1;
        } else {
            bmp_textures.push_back(std::make_unique<bitmap>(this, size, static_cast<int>(bpp)));
            *result = bmp_textures.size();
        }

        *result |= HANDLE_BITMAP;

        // Notify
        helper.finish(this, 0);
    }

    void shared_graphics_driver::bind_bitmap(command_helper &helper) {
        drivers::handle h = 0;
        helper.pop(h);

        if (h == 0) {
            current_fb_height = swapchain_size.y;
            current_fb_width = swapchain_size.x;

            binding = nullptr;

            // bind back to what we used to
            bind_swapchain_framebuf();

            return;
        }

        bitmap *bmp = get_bitmap(h);

        if (!bmp) {
            LOG_ERROR(DRIVER_GRAPHICS, "Bitmap handle invalid to be binded");
            return;
        }

        if (!bmp->fb) {
            // Make new one
            bmp->init_fb(this);
        }

        drivers::handle rh = 0;
        helper.pop(rh);

        drivers::framebuffer_bind_type bind_type = framebuffer_bind_read_draw;
        if ((rh != 0) && (rh != h)) {
            bitmap *bmp_read = get_bitmap(rh);
            if (bmp_read) {
                bmp_read->fb->bind(this, framebuffer_bind_read);
                bind_type = framebuffer_bind_draw;
            }
        }

        // Bind the framebuffer
        bmp->fb->bind(this, bind_type);
        binding = bmp;

        // Build projection matrixx
        projection_matrix = glm::identity<glm::mat4>();
        projection_matrix = glm::ortho(0.0f, static_cast<float>(bmp->tex->get_size().x), static_cast<float>(bmp->tex->get_size().y),
            0.0f, -1.0f, 1.0f);

        current_fb_height = bmp->tex->get_size().y;
        current_fb_width = bmp->tex->get_size().x;

        set_viewport(eka2l1::rect(eka2l1::vec2(0, 0), bmp->tex->get_size()));
    }

    void shared_graphics_driver::read_bitmap(command_helper &helper) {
        drivers::handle handle = 0;
        helper.pop(handle);
        
        bitmap *bmp = get_bitmap(handle);

        if (!bmp) {
            helper.finish(this, 0);
            return;
        }

        eka2l1::point pos(0, 0);
        eka2l1::object_size size(0, 0);
        std::uint32_t bpp = 0;

        helper.pop(pos);
        helper.pop(size);
        helper.pop(bpp);

        texture_format target_format = texture_format::rgba;
        texture_data_type target_data_type = texture_data_type::ubyte;

        switch (bpp) {
        case 8:
        case 24:
        case 32:
            break;

        case 12:
            target_format = texture_format::rgba4;
            target_data_type = texture_data_type::ushort_4_4_4_4;
            break;

        case 16:
            target_format = texture_format::rgb;
            target_data_type = texture_data_type::ushort_5_6_5;
            break;

        default:
            LOG_ERROR(DRIVER_GRAPHICS, "Unsupported BPP type to read format from (value={})", bpp);
            helper.finish(this, 0);

            return;
        }

        std::uint8_t *ptr = nullptr;
        helper.pop(ptr);

        if (!ptr) {
            helper.finish(this, 0);
            return;
        }

        if (!bmp->fb) {
            // Make new one
            bmp->init_fb(this);
        }

        bmp->fb->bind(this, drivers::framebuffer_bind_read_draw);
        const bool res = bmp->fb->read(target_format, target_data_type, pos, size, ptr);
        bmp->fb->unbind(this);
        
        helper.finish(this, res);
    }

    void shared_graphics_driver::destroy_bitmap(command_helper &helper) {
        drivers::handle h = 0;
        helper.pop(h);

        if ((h & ~HANDLE_BITMAP) > bmp_textures.size()) {
            LOG_ERROR(DRIVER_GRAPHICS, "Invalid bitmap handle to destroy");
            return;
        }

        bmp_textures[(h & ~HANDLE_BITMAP) - 1].reset();
    }

    void shared_graphics_driver::resize_bitmap(command_helper &helper) {
        drivers::handle h = 0;
        helper.pop(h);

        bitmap *bmp = get_bitmap(h);

        if (!bmp) {
            LOG_ERROR(DRIVER_GRAPHICS, "Bitmap handle invalid to be binded");
            return;
        }

        vec2 new_size = { 0, 0 };
        helper.pop(new_size);

        // Change texture size
        bmp->resize(this, new_size);
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

    void shared_graphics_driver::create_module(command_helper &helper) {
        const char *data = nullptr;
        std::size_t data_size = 0;
        drivers::shader_module_type mod_type = drivers::shader_module_type::vertex;

        helper.pop(data);
        helper.pop(data_size);
        helper.pop(mod_type);

        auto obj = make_shader_module(this);
        if (!obj->create(this, data, data_size, mod_type)) {
            LOG_ERROR(DRIVER_GRAPHICS, "Fail to create shader module!");
            helper.finish(this, -1);
            return;
        }

        std::unique_ptr<graphics_object> obj_casted = std::move(obj);
        drivers::handle res = append_graphics_object(obj_casted);

        drivers::handle *store = nullptr;
        helper.pop(store);

        *store = res;
        helper.finish(this, 0);
    }

    void shared_graphics_driver::create_program(command_helper &helper) {
        drivers::handle vert_module_handle = 0;
        drivers::handle frag_module_handle = 0;
        void **metadata = nullptr;

        helper.pop(vert_module_handle);
        helper.pop(frag_module_handle);
        helper.pop(metadata);

        auto obj = make_shader_program(this);

        shader_module *vert_module_obj = reinterpret_cast<shader_module*>(get_graphics_object(vert_module_handle));
        shader_module *frag_module_obj = reinterpret_cast<shader_module*>(get_graphics_object(frag_module_handle));

        if (!obj->create(this, vert_module_obj, frag_module_obj)) {
            LOG_ERROR(DRIVER_GRAPHICS, "Fail to create shader program!");
            helper.finish(this, -1);
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

        std::size_t pixels_per_line = 0;
        helper.pop(pixels_per_line);

        drivers::handle h = 0;
        helper.pop(h);

        drivers::texture *obj = nullptr;
        drivers::texture_ptr obj_inst = nullptr;

        if (h != 0) {
            obj = reinterpret_cast<drivers::texture*>(get_graphics_object(h));
            if (!obj) {
                LOG_ERROR(DRIVER_GRAPHICS, "Texture object with handle {} does not exist!", h);
                return;
            }
        } else {
            obj_inst = make_texture(this);
            obj = obj_inst.get();
        }

        obj->create(this, static_cast<int>(dim), static_cast<int>(mip_level), eka2l1::vec3(width, height, depth),
            internal_format, data_format, data_type, data, pixels_per_line);

        if (obj_inst) {
            std::unique_ptr<graphics_object> obj_casted = std::move(obj_inst);
            drivers::handle res = append_graphics_object(obj_casted);

            drivers::handle *store = nullptr;
            helper.pop(store);

            *store = res;
        } else {
            if (data != nullptr) {
                std::uint8_t *data_org = reinterpret_cast<std::uint8_t*>(data);
                delete data_org;
            }
        }

        helper.finish(this, 0);
    }

    void shared_graphics_driver::create_buffer(command_helper &helper) {
        void *initial_data = nullptr;
        std::size_t initial_size = 0;
        buffer_upload_hint upload_hint = static_cast<buffer_upload_hint>(0);
        drivers::handle existing_handle = 0;

        helper.pop(initial_data);
        helper.pop(initial_size);
        helper.pop(upload_hint);
        helper.pop(existing_handle);

        drivers::buffer *obj = nullptr;
        std::unique_ptr<drivers::buffer> obj_inst = nullptr;

        if (existing_handle != 0) {
            obj = reinterpret_cast<drivers::buffer*>(get_graphics_object(existing_handle));
            if (!obj) {
                return;
            }
        } else {    
            obj_inst = make_buffer(this);
            obj = obj_inst.get();
        }

        obj->create(this, initial_data, initial_size, upload_hint);

        if (obj_inst) {
            std::unique_ptr<graphics_object> obj_casted = std::move(obj_inst);
            drivers::handle res = append_graphics_object(obj_casted);

            drivers::handle *store = nullptr;
            helper.pop(store);

            *store = res;

            helper.finish(this, 0);
        } else if ((initial_data != nullptr) && (existing_handle)) {
            std::uint8_t *data_casted = reinterpret_cast<std::uint8_t*>(initial_data);
            delete data_casted;
        }
    }

    void shared_graphics_driver::create_input_descriptors(command_helper &helper) {
        drivers::input_descriptor *descs = nullptr;
        std::uint32_t count = 0;
        drivers::handle existing_handle = 0;

        helper.pop(descs);
        helper.pop(count);
        helper.pop(existing_handle);

        drivers::input_descriptors *obj = nullptr;
        std::unique_ptr<drivers::input_descriptors> obj_inst = nullptr;

        if (existing_handle != 0) {
            obj = reinterpret_cast<drivers::input_descriptors*>(get_graphics_object(existing_handle));
            if (!obj) {
                return;
            }
        } else {    
            obj_inst = make_input_descriptors(this);
            obj = obj_inst.get();
        }

        obj->modify(this, descs, count);

        if (obj_inst) {
            std::unique_ptr<graphics_object> obj_casted = std::move(obj_inst);
            drivers::handle res = append_graphics_object(obj_casted);

            drivers::handle *store = nullptr;
            helper.pop(store);

            *store = res;

            helper.finish(this, 0);
        } else if ((descs != nullptr) && (existing_handle)) {
            std::uint8_t *data_casted = reinterpret_cast<std::uint8_t*>(descs);
            delete data_casted;
        }
    }

    void shared_graphics_driver::use_program(command_helper &helper) {
        drivers::handle num;
        helper.pop(num);

        shader_program *shobj = reinterpret_cast<shader_program *>(get_graphics_object(num));

        if (!shobj) {
            return;
        }

        shobj->use(this);
    }

    void shared_graphics_driver::set_swizzle(command_helper &helper) {
        drivers::handle num = 0;
        drivers::channel_swizzle r = drivers::channel_swizzle::red;
        drivers::channel_swizzle g = drivers::channel_swizzle::green;
        drivers::channel_swizzle b = drivers::channel_swizzle::blue;
        drivers::channel_swizzle a = drivers::channel_swizzle::alpha;

        helper.pop(num);
        helper.pop(r);
        helper.pop(g);
        helper.pop(b);
        helper.pop(a);

        texture *target = nullptr;

        if (num & HANDLE_BITMAP) {
            bitmap *bmp = get_bitmap(num);

            if (!bmp) {
                return;
            }

            target = bmp->tex.get();
        } else {
            target = reinterpret_cast<drivers::texture *>(get_graphics_object(num));
        }

        target->set_channel_swizzle({ r, g, b, a });
    }

    void shared_graphics_driver::bind_texture(command_helper &helper) {
        drivers::handle num = 0;
        int binding = 0;

        helper.pop(num);
        helper.pop(binding);

        if (num & HANDLE_BITMAP) {
            // Bind bitmap as texture
            bitmap *b = get_bitmap(num);

            if (!b) {
                return;
            }

            b->tex->bind(this, binding);
            return;
        }

        texture *texobj = reinterpret_cast<texture *>(get_graphics_object(num));

        if (!texobj) {
            return;
        }

        texobj->bind(this, binding);
    }

    void shared_graphics_driver::update_buffer(command_helper &helper) {
        drivers::handle num = 0;
        std::uint8_t *data = nullptr;
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

    void shared_graphics_driver::destroy_object(command_helper &helper) {
        drivers::handle h = 0;
        helper.pop(h);

        delete_graphics_object(h);
    }

    void shared_graphics_driver::set_filter(command_helper &helper) {
        drivers::handle h = 0;
        bool is_min = false;
        drivers::filter_option filter = drivers::filter_option::linear;

        helper.pop(h);
        helper.pop(is_min);
        helper.pop(filter);

        texture *texobj = nullptr;

        if (h & HANDLE_BITMAP) {
            // Bind bitmap as texture
            bitmap *b = get_bitmap(h);

            if (!b) {
                return;
            }

            texobj = b->tex.get();
        } else {
            texobj = reinterpret_cast<texture *>(get_graphics_object(h));
        }

        if (!texobj) {
            return;
        }

        texobj->set_filter_minmag(is_min, filter);
    }

    void shared_graphics_driver::set_texture_wrap(command_helper &helper) {
        drivers::handle h = 0;
        drivers::addressing_direction dir = drivers::addressing_direction::s;
        drivers::addressing_option mode = drivers::addressing_option::repeat;

        helper.pop(h);
        helper.pop(dir);
        helper.pop(mode);

        texture *texobj = nullptr;

        if (h & HANDLE_BITMAP) {
            // Bind bitmap as texture
            bitmap *b = get_bitmap(h);

            if (!b) {
                return;
            }

            texobj = b->tex.get();
        } else {
            texobj = reinterpret_cast<texture *>(get_graphics_object(h));
        }

        if (!texobj) {
            return;
        }

        texobj->set_addressing_mode(dir, mode);
    }

    void shared_graphics_driver::generate_mips(command_helper &helper) {
        drivers::handle h = 0;

        helper.pop(h);

        texture *texobj = nullptr;

        if (h & HANDLE_BITMAP) {
            // Bind bitmap as texture
            bitmap *b = get_bitmap(h);

            if (!b) {
                return;
            }

            texobj = b->tex.get();
        } else {
            texobj = reinterpret_cast<texture *>(get_graphics_object(h));
        }

        if (!texobj) {
            return;
        }

        texobj->generate_mips();
    }

    void shared_graphics_driver::set_swapchain_size(command_helper &helper) {
        eka2l1::vec2 size;
        helper.pop(size);

        swapchain_size = size;

        if (!binding) {
            current_fb_height = swapchain_size.y;
        }

        projection_matrix = glm::identity<glm::mat4>();
        projection_matrix = glm::ortho(0.0f, static_cast<float>(swapchain_size.x), static_cast<float>(swapchain_size.y),
            0.0f, -1.0f, 1.0f);
    }

    void shared_graphics_driver::set_ortho_size(command_helper &helper) {
        eka2l1::vec2 size;
        helper.pop(size);

        projection_matrix = glm::identity<glm::mat4>();
        projection_matrix = glm::ortho(0.0f, static_cast<float>(size.x), static_cast<float>(size.y),
            0.0f, -1.0f, 1.0f);
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

        case graphics_driver_resize_bitmap: {
            resize_bitmap(helper);
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

        case graphics_driver_create_shader_module:
            create_module(helper);
            break;

        case graphics_driver_create_shader_program: {
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

        case graphics_driver_use_program: {
            use_program(helper);
            break;
        }

        case graphics_driver_bind_texture: {
            bind_texture(helper);
            break;
        }

        case graphics_driver_update_buffer: {
            update_buffer(helper);
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

        case graphics_driver_set_ortho_size:
            set_ortho_size(helper);
            break;

        case graphics_driver_set_swizzle: {
            set_swizzle(helper);
            break;
        }

        case graphics_driver_read_bitmap:
            read_bitmap(helper);
            break;

        case graphics_driver_update_texture:
            update_texture(helper);
            break;

        case graphics_driver_set_texture_wrap:
            set_texture_wrap(helper);
            break;

        case graphics_driver_generate_mips:
            generate_mips(helper);
            break;

        case graphics_driver_create_input_descriptor:
            create_input_descriptors(helper);
            break;

        default:
            LOG_ERROR(DRIVER_GRAPHICS, "Unimplemented opcode {} for graphics driver", cmd->opcode_);
            break;
        }
    }
}