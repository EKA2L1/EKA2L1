#pragma once

#include <common/vecx.h>
#include <drivers/graphics/texture.h>

namespace eka2l1::drivers {
    class ogl_texture : public texture {
        friend class ogl_graphics_driver;

        int dimensions;
        vec3 tex_size;
        texture_format internal_format;
        texture_format format;
        texture_data_type tex_data_type;
        std::size_t pixels_per_line;

        std::uint32_t texture{ 0 };
        int last_tex{ 0 };
        int last_active{ 0 };

    public:
        ogl_texture() {}
        ~ogl_texture() override;

        bool create(graphics_driver *driver, const int dim, const int miplvl, const vec3 &size, const texture_format internal_format,
            const texture_format format, const texture_data_type data_type, void *data, const std::size_t data_size,
            const std::size_t pixels_per_line = 0, const std::uint32_t unpack_alignment = 4) override;

        void set_filter_minmag(const bool min, const filter_option op) override;
        void set_addressing_mode(const addressing_direction dir, const addressing_option op) override;
        void set_channel_swizzle(channel_swizzles swizz) override;
        void generate_mips() override;
        void set_max_mip_level(const std::uint32_t max_mip) override;

        void bind(graphics_driver *driver, const int binding) override;
        void unbind(graphics_driver *driver) override;

        void update_data(graphics_driver *driver, const int mip_lvl, const vec3 &offset, const vec3 &size, const std::size_t byte_width,
            const texture_format data_format, const texture_data_type data_type, const void *data, const std::size_t data_size, const std::uint32_t unpack_alignment) override;

        vec2 get_size() const override {
            return tex_size;
        }

        texture_format get_format() const override {
            return internal_format;
        }

        texture_data_type get_data_type() const override {
            return tex_data_type;
        }

        int get_total_dimensions() const override {
            return dimensions;
        }

        std::uint64_t driver_handle() override {
            return texture;
        }
    };
    
    class ogl_renderbuffer: public renderbuffer {
    private:
        vec2 tex_size;
        texture_format internal_format;

        std::uint32_t handle{ 0 };
        std::int32_t last_renderbuffer{ 0 };

    public:
        bool create(graphics_driver *driver, const vec2 &size, const texture_format format) override;

        explicit ogl_renderbuffer();
        ~ogl_renderbuffer() override;

        void bind(graphics_driver *driver, const int binding) override;
        void unbind(graphics_driver *driver) override;
        
        vec2 get_size() const override {
            return tex_size;
        }

        texture_format get_format() const override {
            return internal_format;
        }

        std::uint64_t driver_handle() override {
            return handle;
        }
    };
}
