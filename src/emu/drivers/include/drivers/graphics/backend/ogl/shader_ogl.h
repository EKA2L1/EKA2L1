#pragma once

#include <drivers/graphics/shader.h>

#include <cstdint>
#include <string>

namespace eka2l1::drivers {
    class ogl_shader: public shader {
        std::uint32_t program;
    public:
        ogl_shader() : program(0) {}

        explicit ogl_shader(const std::string &vert_path, 
            const std::string &frag_path);

        explicit ogl_shader(const char *vert_data, const std::size_t vert_size,
            const char *frag_data, const std::size_t frag_size);

        bool create(const char *vert_data, const std::size_t vert_size,
            const char *frag_data, const std::size_t frag_size) override;

        bool use() override;

        std::uint32_t program_handle() const {
            return program;
        }

        std::optional<int> get_uniform_location(const std::string &name) override;
        std::optional<int> get_attrib_location(const std::string &name) override;
    };
}