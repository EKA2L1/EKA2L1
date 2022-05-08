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

        texture->create(driver, 2, 0, eka2l1::vec3(size.x, size.y, 0), internal_format, data_format, data_type, nullptr, 0);
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
        ds_tex.reset();
        fb.reset();
    }

    void bitmap::resize(graphics_driver *driver, const eka2l1::vec2 &new_size) {
        auto tex_to_replace = std::move(instantiate_suit_color_bitmap_texture(driver, new_size, bpp));
        auto ds_tex_to_replace = std::move(instantiate_bitmap_depth_stencil_texture(driver, new_size));

        if (fb) {
            auto new_fb = make_framebuffer(driver, { tex_to_replace.get() }, { 0 }, ds_tex_to_replace.get(), 0,
                ds_tex_to_replace.get(), 0);

            fb->bind(driver, framebuffer_bind_read);
            new_fb->bind(driver, framebuffer_bind_draw);

            eka2l1::rect source_region;
            source_region.top = { 0, 0 };
            source_region.size = tex->get_size();

            eka2l1::rect dest_region;
            dest_region.top = { 0, 0 };;
            dest_region.size = new_size;

            fb->blit(source_region, dest_region, draw_buffer_bit_color_buffer, filter_option::linear);

            new_fb->unbind(driver);
            fb->unbind(driver);

            fb = std::move(new_fb);
        }

        tex = std::move(tex_to_replace);
        ds_tex = std::move(ds_tex_to_replace);
    }

    void bitmap::init_fb(graphics_driver *driver) {
        ds_tex = std::move(instantiate_bitmap_depth_stencil_texture(driver, tex->get_size()));
        fb = make_framebuffer(driver, { tex.get() }, { 0 }, ds_tex.get(), 0, ds_tex.get(), 0);
    }

    shared_graphics_driver::shared_graphics_driver(const graphic_api gr_api)
        : graphics_driver(gr_api)
        , binding(nullptr)
        , brush_color({ 255.0f, 255.0f, 255.0f, 255.0f })
        , current_fb_height(0) {
    }

    shared_graphics_driver::~shared_graphics_driver() {
    }

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
            data_format, data_type, data, 0, 4);

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

    void shared_graphics_driver::update_bitmap(command &cmd) {
        drivers::handle handle = cmd.data_[0];
        std::uint8_t *data = reinterpret_cast<std::uint8_t*>(cmd.data_[1]);
        std::size_t size = static_cast<std::size_t>(cmd.data_[2]);
        eka2l1::vec2 offset;
        eka2l1::vec2 dim;
        std::size_t pixels_per_line = static_cast<std::size_t>(cmd.data_[5]);

        unpack_u64_to_2u32(cmd.data_[3], offset.x, offset.y);
        unpack_u64_to_2u32(cmd.data_[4], dim.x, dim.y);

        update_bitmap(handle, size, offset, dim, data, pixels_per_line);

        delete[] data;
    }

    void shared_graphics_driver::update_texture(command &cmd) {
        drivers::handle handle = cmd.data_[0];
        std::uint8_t *data = reinterpret_cast<std::uint8_t*>(cmd.data_[1]);
        std::size_t size = static_cast<std::size_t>(cmd.data_[2]);
        std::uint8_t lvl = static_cast<std::uint8_t>(cmd.data_[3]);
        drivers::texture_format data_format = static_cast<drivers::texture_format>(cmd.data_[3] >> 8);
        drivers::texture_data_type data_type = static_cast<drivers::texture_data_type>(cmd.data_[3] >> 24);
        eka2l1::vec3 offset;
        eka2l1::vec3 dim;
        std::size_t pixels_per_line = cmd.data_[7];
        std::uint32_t unpack_alignment = static_cast<std::uint32_t>(cmd.data_[8]);

        unpack_u64_to_2u32(cmd.data_[4], offset.x, offset.y);
        unpack_u64_to_2u32(cmd.data_[5], offset.z, dim.x);
        unpack_u64_to_2u32(cmd.data_[6], dim.y, dim.z);

        drivers::texture *obj = reinterpret_cast<drivers::texture*>(get_graphics_object(handle));
        if (!obj) {
            return;
        }

        obj->update_data(this, static_cast<int>(lvl), offset, dim, pixels_per_line, data_format, data_type, data, size, unpack_alignment);

        delete[] data;
    }

    void shared_graphics_driver::create_bitmap(command &cmd) {
        eka2l1::vec2 size;
        std::uint32_t bpp = static_cast<std::uint32_t>(cmd.data_[1]);
        drivers::handle *result = reinterpret_cast<drivers::handle*>(cmd.data_[2]);

        unpack_u64_to_2u32(cmd.data_[0], size.x, size.y);

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
        finish(cmd.status_, 0);
    }

    void shared_graphics_driver::bind_bitmap(command &cmd) {
        drivers::handle h = cmd.data_[0];

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

        drivers::handle rh = cmd.data_[1];

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

        bool no_need_flip = !(!binding && (get_current_api() == drivers::graphic_api::opengl));

        // Build projection matrixx
        projection_matrix = glm::identity<glm::mat4>();
        projection_matrix = glm::ortho(0.0f, static_cast<float>(bmp->tex->get_size().x), no_need_flip ? static_cast<float>(bmp->tex->get_size().y) : 0,
            no_need_flip ? 0.0f : static_cast<float>(bmp->tex->get_size().y), -1.0f, 1.0f);

        current_fb_height = bmp->tex->get_size().y;
        current_fb_width = bmp->tex->get_size().x;

        set_viewport(eka2l1::rect(eka2l1::vec2(0, 0), bmp->tex->get_size()));
    }

    void shared_graphics_driver::read_bitmap(command &cmd) {
        drivers::handle handle = cmd.data_[0];
        
        bitmap *bmp = get_bitmap(handle);

        if (!bmp) {
            finish(cmd.status_, 0);
            return;
        }

        eka2l1::point pos(0, 0);
        eka2l1::object_size size(0, 0);
        std::uint32_t bpp = static_cast<std::uint32_t>(cmd.data_[3]);

        unpack_u64_to_2u32(cmd.data_[1], pos.x, pos.y);
        unpack_u64_to_2u32(cmd.data_[2], size.x, size.y);

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
            finish(cmd.status_, 0);

            return;
        }

        std::uint8_t *ptr = reinterpret_cast<std::uint8_t*>(cmd.data_[4]);

        if (!ptr) {
            finish(cmd.status_, 0);
            return;
        }

        if (!bmp->fb) {
            // Make new one
            bmp->init_fb(this);
        }

        bmp->fb->bind(this, drivers::framebuffer_bind_read_draw);
        const bool res = bmp->fb->read(target_format, target_data_type, pos, size, ptr);
        bmp->fb->unbind(this);

        finish(cmd.status_, res);
    }

    void shared_graphics_driver::destroy_bitmap(command &cmd) {
        drivers::handle h = cmd.data_[0];

        if ((h & ~HANDLE_BITMAP) > bmp_textures.size()) {
            LOG_ERROR(DRIVER_GRAPHICS, "Invalid bitmap handle to destroy");
            return;
        }

        bmp_textures[(h & ~HANDLE_BITMAP) - 1].reset();
    }

    void shared_graphics_driver::resize_bitmap(command &cmd) {
        drivers::handle h = cmd.data_[0];

        bitmap *bmp = get_bitmap(h);

        if (!bmp) {
            LOG_ERROR(DRIVER_GRAPHICS, "Bitmap handle invalid to be binded");
            return;
        }

        vec2 new_size = { 0, 0 };
        unpack_u64_to_2u32(cmd.data_[1], new_size.x, new_size.y);

        // Change texture size
        bmp->resize(this, new_size);
    }

    void shared_graphics_driver::set_brush_color(command &cmd) {
        std::uint32_t r,g,b,a;
        unpack_u64_to_2u32(cmd.data_[0], r, g);
        unpack_u64_to_2u32(cmd.data_[1], b, a);

        brush_color = { static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a) };
    }

    void shared_graphics_driver::create_module(command &cmd) {
        const char *data = reinterpret_cast<const char*>(cmd.data_[0]);
        std::size_t data_size = static_cast<std::size_t>(cmd.data_[1]);
        drivers::shader_module_type mod_type = static_cast<drivers::shader_module_type>(cmd.data_[2]);
        std::string *compile_log = reinterpret_cast<std::string*>(cmd.data_[4]);

        auto obj = make_shader_module(this);
        if (!obj->create(this, data, data_size, mod_type, compile_log)) {
            LOG_ERROR(DRIVER_GRAPHICS, "Fail to create shader module!");
            finish(cmd.status_, -1);
            return;
        }

        std::unique_ptr<graphics_object> obj_casted = std::move(obj);
        drivers::handle res = append_graphics_object(obj_casted);

        drivers::handle *store = reinterpret_cast<drivers::handle*>(cmd.data_[3]);
        *store = res;

        finish(cmd.status_, 0);
    }

    void shared_graphics_driver::create_program(command &cmd) {
        drivers::handle vert_module_handle = static_cast<drivers::handle>(cmd.data_[0]);
        drivers::handle frag_module_handle = static_cast<drivers::handle>(cmd.data_[1]);
        void **metadata = reinterpret_cast<void**>(cmd.data_[2]);
        std::string *link_log = reinterpret_cast<std::string*>(cmd.data_[4]);

        auto obj = make_shader_program(this);

        shader_module *vert_module_obj = reinterpret_cast<shader_module*>(get_graphics_object(vert_module_handle));
        shader_module *frag_module_obj = reinterpret_cast<shader_module*>(get_graphics_object(frag_module_handle));

        if (!obj->create(this, vert_module_obj, frag_module_obj, link_log)) {
            LOG_ERROR(DRIVER_GRAPHICS, "Fail to create shader program!");
            finish(cmd.status_, -1);
            return;
        }

        if (metadata) {
            *metadata = obj->get_metadata();
        }

        std::unique_ptr<graphics_object> obj_casted = std::move(obj);
        drivers::handle res = append_graphics_object(obj_casted);

        drivers::handle *store = reinterpret_cast<drivers::handle*>(cmd.data_[3]);

        *store = res;
        finish(cmd.status_, 0);
    }

    void shared_graphics_driver::create_texture(command &cmd) {
        std::uint8_t dim = static_cast<std::uint8_t>(cmd.data_[0]);
        std::uint8_t mip_level = static_cast<std::uint8_t>(cmd.data_[0] >> 8);

        drivers::texture_format internal_format = static_cast<drivers::texture_format>(cmd.data_[0] >> 16);
        drivers::texture_format data_format = static_cast<drivers::texture_format>(cmd.data_[0] >> 32);
        drivers::texture_data_type data_type = static_cast<drivers::texture_data_type>(cmd.data_[0] >> 48);
        void *data = reinterpret_cast<void*>(cmd.data_[1]);
        std::size_t data_size = static_cast<std::size_t>(cmd.data_[2]);

        std::size_t pixels_per_line = static_cast<std::size_t>(cmd.data_[3]);
        std::uint32_t alignment = static_cast<std::uint32_t>(cmd.data_[4]);

        std::int32_t width, height, depth, temp;
        unpack_u64_to_2u32(cmd.data_[5], width, height);
        unpack_u64_to_2u32(cmd.data_[6], depth, temp);

        drivers::handle h = static_cast<drivers::handle>(cmd.data_[7]);

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
            internal_format, data_format, data_type, data, data_size, pixels_per_line, alignment);

        if (obj_inst) {
            std::unique_ptr<graphics_object> obj_casted = std::move(obj_inst);
            drivers::handle res = append_graphics_object(obj_casted);

            drivers::handle *store = reinterpret_cast<drivers::handle*>(cmd.data_[8]);
            *store = res;
        } else {
            if (data != nullptr) {
                std::uint8_t *data_org = reinterpret_cast<std::uint8_t*>(data);
                delete[] data_org;
            }
        }

        finish(cmd.status_, 0);
    }

    void shared_graphics_driver::create_buffer(command &cmd) {
        void *initial_data = reinterpret_cast<void*>(cmd.data_[0]);
        std::size_t initial_size = static_cast<std::size_t>(cmd.data_[1]);
        buffer_upload_hint upload_hint = static_cast<buffer_upload_hint>(cmd.data_[2]);
        drivers::handle existing_handle = static_cast<drivers::handle>(cmd.data_[3]);

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

            drivers::handle *store = reinterpret_cast<drivers::handle*>(cmd.data_[4]);
            *store = res;

            finish(cmd.status_, 0);
        } else if ((initial_data != nullptr) && (existing_handle)) {
            std::uint8_t *data_casted = reinterpret_cast<std::uint8_t*>(initial_data);
            delete[] data_casted;
        }
    }

    void shared_graphics_driver::create_input_descriptors(command &cmd) {
        drivers::input_descriptor *descs = reinterpret_cast<drivers::input_descriptor*>(cmd.data_[0]);
        std::uint32_t count = static_cast<std::uint32_t>(cmd.data_[1]);
        drivers::handle existing_handle = static_cast<drivers::handle>(cmd.data_[2]);

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

            drivers::handle *store = reinterpret_cast<drivers::handle*>(cmd.data_[3]);
            *store = res;

            finish(cmd.status_, 0);
        } else if ((descs != nullptr) && (existing_handle)) {
            std::uint8_t *data_casted = reinterpret_cast<std::uint8_t*>(descs);
            delete[] data_casted;
        }
    }

    void shared_graphics_driver::create_renderbuffer(command &cmd) {
        eka2l1::vec2 size;
        drivers::texture_format internal_format = drivers::texture_format::none;
        drivers::handle existing_handle = cmd.data_[2];

        unpack_u64_to_2u32(cmd.data_[0], size.x, size.y);
        internal_format = static_cast<drivers::texture_format>(cmd.data_[1]);
        
        drivers::renderbuffer *obj = nullptr;
        std::unique_ptr<drivers::renderbuffer> obj_inst = nullptr;

        if (existing_handle != 0) {
            obj = reinterpret_cast<drivers::renderbuffer*>(get_graphics_object(existing_handle));
            if (!obj) {
                return;
            }
        } else {    
            obj_inst = make_renderbuffer(this);
            obj = obj_inst.get();
        }

        obj->create(this, size, internal_format);
        
        if (obj_inst) {
            std::unique_ptr<graphics_object> obj_casted = std::move(obj_inst);
            drivers::handle res = append_graphics_object(obj_casted);

            drivers::handle *store = reinterpret_cast<drivers::handle*>(cmd.data_[3]);
            *store = res;

            finish(cmd.status_, 0);
        }
    }

    void shared_graphics_driver::create_framebuffer(command &cmd) {
        drivers::handle *color_buffers = reinterpret_cast<drivers::handle*>(cmd.data_[0]);
        int *color_face_indicies = reinterpret_cast<int*>(cmd.data_[1]);
        std::uint32_t color_buffer_count = static_cast<std::uint32_t>(cmd.data_[2]);
        drivers::handle depth_buffer = static_cast<drivers::handle>(cmd.data_[3]);
        drivers::handle stencil_buffer = static_cast<drivers::handle>(cmd.data_[4]);

        int depth_face_index, stencil_face_index;
        unpack_u64_to_2u32(cmd.data_[5], depth_face_index, stencil_face_index);

        std::vector<drawable*> color_buffer_objs;
        std::vector<int> color_face_indicies_v;

        drawable *temp = nullptr;

        for (std::uint32_t i = 0; i < color_buffer_count; i++) {
            temp = reinterpret_cast<drawable*>(get_graphics_object(color_buffers[i]));
            if (!temp) {
                return;
            }

            color_buffer_objs.push_back(temp);
            color_face_indicies_v.push_back(color_face_indicies[i]);
        }

        drawable *depth_buffer_obj = nullptr;
        drawable *stencil_buffer_obj = nullptr;

        if (depth_buffer) {
            depth_buffer_obj = reinterpret_cast<drawable*>(get_graphics_object(depth_buffer));
            if (!depth_buffer_obj) {
                return;
            }
        }

        if (stencil_buffer) {
            stencil_buffer_obj = reinterpret_cast<drawable*>(get_graphics_object(stencil_buffer));
            if (!stencil_buffer_obj) {
                return;
            }
        }

        framebuffer_ptr new_fb = make_framebuffer(this, color_buffer_objs, color_face_indicies_v, depth_buffer_obj,
            depth_face_index, stencil_buffer_obj, stencil_face_index);

        std::unique_ptr<graphics_object> obj_casted = std::move(new_fb);
        drivers::handle res = append_graphics_object(obj_casted);

        drivers::handle *store = reinterpret_cast<drivers::handle*>(cmd.data_[6]);
        *store = res;

        finish(cmd.status_, 0);
    }

    void shared_graphics_driver::use_program(command &cmd) {
        drivers::handle num = static_cast<drivers::handle>(cmd.data_[0]);
        shader_program *shobj = reinterpret_cast<shader_program *>(get_graphics_object(num));

        if (!shobj) {
            return;
        }

        shobj->use(this);
    }

    void shared_graphics_driver::set_swizzle(command &cmd) {
        drivers::handle num = static_cast<drivers::handle>(cmd.data_[0]);
        drivers::channel_swizzle r = drivers::channel_swizzle::red;
        drivers::channel_swizzle g = drivers::channel_swizzle::green;
        drivers::channel_swizzle b = drivers::channel_swizzle::blue;
        drivers::channel_swizzle a = drivers::channel_swizzle::alpha;

        unpack_u64_to_2u32(cmd.data_[1], r, g);
        unpack_u64_to_2u32(cmd.data_[2], b, a);

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

    void shared_graphics_driver::bind_texture(command &cmd) {
        drivers::handle num = static_cast<drivers::handle>(cmd.data_[0]);
        int binding = static_cast<int>(cmd.data_[1]);

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

    void shared_graphics_driver::update_buffer(command &cmd) {
        drivers::handle num = static_cast<drivers::handle>(cmd.data_[0]);
        std::uint8_t *data = reinterpret_cast<std::uint8_t*>(cmd.data_[1]);
        std::size_t offset = static_cast<std::size_t>(cmd.data_[2]);
        std::size_t size = static_cast<std::size_t>(cmd.data_[3]);

        buffer *bufobj = reinterpret_cast<buffer *>(get_graphics_object(num));

        if (!bufobj) {
            return;
        }

        bufobj->update_data(this, data, offset, size);

        delete[] data;
    }

    void shared_graphics_driver::destroy_object(command &cmd) {
        drivers::handle h = static_cast<drivers::handle>(cmd.data_[0]);
        delete_graphics_object(h);
    }

    void shared_graphics_driver::set_filter(command &cmd) {
        drivers::handle h = static_cast<drivers::handle>(cmd.data_[0]);
        bool is_min = false;
        drivers::filter_option filter = drivers::filter_option::linear;

        unpack_u64_to_2u32(cmd.data_[1], is_min, filter);

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

    void shared_graphics_driver::set_texture_wrap(command &cmd) {
        drivers::handle h = static_cast<drivers::handle>(cmd.data_[0]);
        drivers::addressing_direction dir = drivers::addressing_direction::s;
        drivers::addressing_option mode = drivers::addressing_option::repeat;

        unpack_u64_to_2u32(cmd.data_[1], dir, mode);

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

    void shared_graphics_driver::set_max_mip_level(command &cmd) {
        drivers::handle h = static_cast<drivers::handle>(cmd.data_[0]);
        std::uint32_t max_mip = static_cast<std::uint32_t>(cmd.data_[1]);

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

        texobj->set_max_mip_level(max_mip);
    }

    void shared_graphics_driver::generate_mips(command &cmd) {
        drivers::handle h = static_cast<drivers::handle>(cmd.data_[0]);
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

    void shared_graphics_driver::set_swapchain_size(command &cmd) {
        eka2l1::vec2 size;
        unpack_u64_to_2u32(cmd.data_[0], size.x, size.y);

        swapchain_size = size;

        if (!binding) {
            current_fb_height = swapchain_size.y;
        }

        bool no_need_flip = !(!binding && (get_current_api() == drivers::graphic_api::opengl));

        projection_matrix = glm::identity<glm::mat4>();
        projection_matrix = glm::ortho(0.0f, static_cast<float>(swapchain_size.x), no_need_flip ? static_cast<float>(swapchain_size.y) : 0.0f,
            no_need_flip ? 0.0f : static_cast<float>(swapchain_size.y), -1.0f, 1.0f);
    }

    void shared_graphics_driver::set_ortho_size(command &cmd) {
        eka2l1::vec2 size;
        unpack_u64_to_2u32(cmd.data_[0], size.x, size.y);

        bool no_need_flip = !(!binding && (get_current_api() == drivers::graphic_api::opengl));

        projection_matrix = glm::identity<glm::mat4>();
        projection_matrix = glm::ortho(0.0f, static_cast<float>(size.x), no_need_flip ? static_cast<float>(size.y) : 0,
            no_need_flip ? 0.0f : static_cast<float>(swapchain_size.y), -1.0f, 1.0f);
    }

    void shared_graphics_driver::set_fb_color_buffer(command &cmd) {
        drivers::handle h = cmd.data_[0];

        if (h == 0) {
            return;
        }

        drivers::handle color_buffer = cmd.data_[1];
        int face_index = static_cast<int>(cmd.data_[2]);
        std::int32_t pos = static_cast<std::int32_t>(cmd.data_[3]);

        drivers::framebuffer *fb = reinterpret_cast<drivers::framebuffer*>(get_graphics_object(h));
        if (!fb) {
            return;
        }

        drivers::drawable *obj = nullptr;

        if (color_buffer != 0) {
            obj = reinterpret_cast<drivers::drawable*>(get_graphics_object(color_buffer));
            if (!obj) {
                return;
            }
        }

        fb->bind(nullptr, drivers::framebuffer_bind_read_draw);
        fb->set_color_buffer(obj, face_index, pos);
        fb->unbind(nullptr);
    }

    void shared_graphics_driver::set_fb_depth_stencil_buffer(command &cmd) {
        drivers::handle h = cmd.data_[0];

        if (h == 0) {
            return;
        }

        drivers::handle depth_buffer = cmd.data_[1];
        drivers::handle stencil_buffer = cmd.data_[2];

        int depth_face_index, stencil_face_index;
        unpack_u64_to_2u32(cmd.data_[3], depth_face_index, stencil_face_index);

        drivers::framebuffer *fb = reinterpret_cast<drivers::framebuffer*>(get_graphics_object(h));
        drivers::drawable *depth_buffer_obj = nullptr;
        drivers::drawable *stencil_buffer_obj = nullptr;

        if (depth_buffer != 0) {
            depth_buffer_obj = reinterpret_cast<drivers::drawable*>(get_graphics_object(depth_buffer));
            if (!depth_buffer_obj) {
                return;
            }
        }

        if (stencil_buffer != 0) {
            stencil_buffer_obj = reinterpret_cast<drivers::drawable*>(get_graphics_object(stencil_buffer));
            if (!stencil_buffer_obj) {
                return;
            }
        }

        fb->bind(nullptr, drivers::framebuffer_bind_read_draw);
        fb->set_depth_stencil_buffer(depth_buffer_obj, stencil_buffer_obj, depth_face_index, stencil_face_index);
    }

    void shared_graphics_driver::dispatch(command &cmd) {
        switch (cmd.opcode_) {
        case graphics_driver_create_bitmap: {
            create_bitmap(cmd);
            break;
        }

        case graphics_driver_bind_bitmap: {
            bind_bitmap(cmd);
            break;
        }

        case graphics_driver_update_bitmap: {
            update_bitmap(cmd);
            break;
        }

        case graphics_driver_resize_bitmap: {
            resize_bitmap(cmd);
            break;
        }

        case graphics_driver_destroy_bitmap: {
            destroy_bitmap(cmd);
            break;
        }

        case graphics_driver_destroy_object: {
            destroy_object(cmd);
            break;
        }

        case graphics_driver_set_brush_color: {
            set_brush_color(cmd);
            break;
        }

        case graphics_driver_create_shader_module:
            create_module(cmd);
            break;

        case graphics_driver_create_shader_program: {
            create_program(cmd);
            break;
        }

        case graphics_driver_create_texture: {
            create_texture(cmd);
            break;
        }

        case graphics_driver_create_buffer: {
            create_buffer(cmd);
            break;
        }

        case graphics_driver_create_renderbuffer: {
            create_renderbuffer(cmd);
            break;
        }

        case graphcis_driver_create_framebuffer:
            create_framebuffer(cmd);
            break;

        case graphics_driver_use_program: {
            use_program(cmd);
            break;
        }

        case graphics_driver_bind_texture: {
            bind_texture(cmd);
            break;
        }

        case graphics_driver_update_buffer: {
            update_buffer(cmd);
            break;
        }

        case graphics_driver_set_texture_filter: {
            set_filter(cmd);
            break;
        }

        case graphics_driver_set_swapchain_size: {
            set_swapchain_size(cmd);
            break;
        }

        case graphics_driver_set_ortho_size:
            set_ortho_size(cmd);
            break;

        case graphics_driver_set_swizzle: {
            set_swizzle(cmd);
            break;
        }

        case graphics_driver_read_bitmap:
            read_bitmap(cmd);
            break;

        case graphics_driver_update_texture:
            update_texture(cmd);
            break;

        case graphics_driver_set_texture_wrap:
            set_texture_wrap(cmd);
            break;

        case graphics_driver_generate_mips:
            generate_mips(cmd);
            break;

        case graphics_driver_create_input_descriptor:
            create_input_descriptors(cmd);
            break;

        case graphics_driver_set_max_mip_level:
            set_max_mip_level(cmd);
            break;

        case graphics_driver_set_framebuffer_color_buffer:
            set_fb_color_buffer(cmd);
            break;

        case graphics_driver_set_framebuffer_depth_stencil_buffer:
            set_fb_depth_stencil_buffer(cmd);
            break;

        default:
            LOG_ERROR(DRIVER_GRAPHICS, "Unimplemented opcode {} for graphics driver", cmd.opcode_);
            break;
        }
    }
}