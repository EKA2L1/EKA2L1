#pragma once

#include <common/vecx.h>
#include <drivers/graphics/texture.h>

namespace eka2l1::drivers {
    class ogl_texture : public texture {
        int dimensions;
        vec3 tex_size;
        texture_format internal_format;
        texture_format format;
        texture_data_type tex_data_type;
        void *tex_data;
        int mip_level;

        std::uint32_t texture;

        bool tex();

    public:
        ogl_texture() {}
        ~ogl_texture() override;

        bool create(const int dim, const int miplvl, const vec3 &size, const texture_format internal_format,
            const texture_format format, const texture_data_type data_type, void *data) override;

        void change_size(const vec3 &new_size) override;
        void change_data(const texture_data_type data_type, void *data) override;
        void change_texture_format(const texture_format internal_format, const texture_format format) override;

        void set_filter_minmag(const bool min, const filter_option op) override;

        void bind() override;
        void unbind() override;

        vec2 get_size() const override {
            return tex_size;
        }

        texture_format get_format() const override {
            return internal_format;
        }

        texture_data_type get_data_type() const override {
            return tex_data_type;
        }

        int get_mip_level() const override {
            return mip_level;
        }

        int get_total_dimensions() const override {
            return dimensions;
        }

        void *get_data_ptr() const override {
            return tex_data;
        }

        std::uint32_t texture_handle() {
            return texture;
        }
    };
}