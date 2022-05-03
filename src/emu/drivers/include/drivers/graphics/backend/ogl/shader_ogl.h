#pragma once

#include <drivers/graphics/shader.h>

#include <cstdint>
#include <string>

namespace eka2l1::drivers {
    class ogl_shader_module : public shader_module {
    private:
        std::uint32_t shader;

    public:
        ~ogl_shader_module() override;

        explicit ogl_shader_module();
        explicit ogl_shader_module(const std::string &path, const shader_module_type type, const std::string &extra_header = "");
        explicit ogl_shader_module(const char *data, const std::size_t size, const shader_module_type type);

        bool create(graphics_driver *driver, const char *data, const std::size_t size, const shader_module_type type,
            std::string *compile_log = nullptr) override;

        std::uint32_t shader_handle() const {
            return shader;
        }
    };

    class ogl_shader_program: public shader_program {
    protected:
        std::uint32_t program;
        std::uint8_t *metadata;

    public:
        explicit ogl_shader_program();
        ~ogl_shader_program() override;

        bool create(graphics_driver *driver, shader_module *vertex_module, shader_module *fragment_module, std::string *link_log = nullptr) override;
        bool use(graphics_driver *driver) override;

        std::uint32_t program_handle() const {
            return program;
        }

        std::optional<int> get_uniform_location(const std::string &name) override;
        std::optional<int> get_attrib_location(const std::string &name) override;

        void *get_metadata() override;
    };
}
