/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <drivers/graphics/backend/ogl/shader_ogl.h>
#include <glad/glad.h>

#include <common/buffer.h>
#include <common/log.h>

#include <fstream>
#include <sstream>

namespace eka2l1::drivers {
    /**
      * \brief Build a shader program's metadata. This is for client who desired performance, traded for memory.
      */
    static void build_metadata(GLuint program, std::vector<std::uint8_t> &data) {
        GLint total_attributes = 0;
        GLint total_uniforms = 0;

        glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &total_attributes);
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &total_uniforms);

        data.resize(8);

        reinterpret_cast<std::uint16_t *>(&data[0])[0] = 8;

        GLchar buf[257];
        GLint size = 0;
        GLsizei name_len = 0;
        GLenum type = 0;

        for (int i = 0; i < total_attributes; i++) {
            glGetActiveAttrib(program, i, 256, &name_len, &size, &type, buf);

            // Push
            data.push_back(static_cast<std::uint8_t>(name_len));
            data.insert(data.end(), buf, buf + name_len);

            buf[name_len] = '\0';

            std::int32_t location = glGetAttribLocation(program, buf);
            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&location), reinterpret_cast<std::uint8_t *>(&location) + sizeof(location));
        }

        reinterpret_cast<std::uint16_t *>(&data[0])[1] = static_cast<std::uint16_t>(data.size());
        reinterpret_cast<std::uint16_t *>(&data[0])[2] = static_cast<std::uint16_t>(total_attributes);
        reinterpret_cast<std::uint16_t *>(&data[0])[3] = static_cast<std::uint16_t>(total_uniforms);

        for (int i = 0; i < total_uniforms; i++) {
            glGetActiveUniform(program, i, 256, &name_len, &size, &type, buf);

            buf[name_len] = '\0';

            // Push
            data.push_back(static_cast<std::uint8_t>(name_len));
            data.insert(data.end(), buf, buf + name_len);

            std::int32_t location = glGetUniformLocation(program, buf);
            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&location), reinterpret_cast<std::uint8_t *>(&location) + sizeof(location));
        }
    }

    ogl_shader_module::ogl_shader_module()
        : shader(0) {
    }

    ogl_shader_module::ogl_shader_module(const std::string &path, const shader_module_type type)
        : shader(0) {
        common::ro_std_file_stream stream(path, std::ios_base::binary);
        
        std::string whole_code;
        whole_code.resize(stream.size());

        stream.read(whole_code.data(), whole_code.size());

        create(nullptr, whole_code.data(), whole_code.size(), type);
    }

    ogl_shader_module::ogl_shader_module(const char *data, const std::size_t size, const shader_module_type type)
        : shader(0) {
        create(nullptr, data, size, type);
    }

    ogl_shader_module::~ogl_shader_module() {
        if (shader)
            glDeleteShader(shader);
    }

    bool ogl_shader_module::create(graphics_driver *driver, const char *data, const std::size_t size, const shader_module_type type) {
        shader = glCreateShader(((type == shader_module_type::vertex) ? GL_VERTEX_SHADER :
            ((type == shader_module_type::fragment) ? GL_FRAGMENT_SHADER : GL_GEOMETRY_SHADER)));

        glShaderSource(shader, 1, &data, nullptr);
        glCompileShader(shader);

        int success;
        char error[512] = { '\0' };

        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success) {
            glGetShaderInfoLog(shader, 512, nullptr, error);
            LOG_ERROR(DRIVER_GRAPHICS, "Error while compiling shader: {}, abort", error);

            glDeleteShader(shader);
            return false;
        }

        if (!success) {
            return false;
        }

        return true;
    }

    ogl_shader_program::ogl_shader_program()
        : program(0)
        , metadata(0) {
    }

    ogl_shader_program::~ogl_shader_program() {
        if (metadata) {
            delete metadata;
        }

        if (program) {
            glDeleteProgram(program);
        }
    }

    bool ogl_shader_program::create(graphics_driver *driver, shader_module *vertex_module, shader_module *fragment_module) {
        ogl_shader_module *ogl_vertex_module = reinterpret_cast<ogl_shader_module*>(vertex_module);
        ogl_shader_module *ogl_fragment_module = reinterpret_cast<ogl_shader_module*>(fragment_module);

        if (!ogl_vertex_module || !ogl_fragment_module) {
            return false;
        }

        if (program) {
            glDeleteProgram(program);
        }

        program = glCreateProgram();
        glAttachShader(program, ogl_vertex_module->shader_handle());
        glAttachShader(program, ogl_fragment_module->shader_handle());

        GLint success = 0;

        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &success);

        if (!success) {
            char error[512] = { '\0' };

            glGetProgramInfoLog(program, 512, nullptr, error);
            LOG_ERROR(DRIVER_GRAPHICS, "Error while linking shader program: {}", error);

            glDeleteProgram(program);
            return false;
        }

        return true;
    }

    bool ogl_shader_program::use(graphics_driver *driver) {
        glUseProgram(program);
        return true;
    }

    std::optional<int> ogl_shader_program::get_uniform_location(const std::string &name) {
        const auto res = glGetUniformLocation(program, name.c_str());

        if (res == -1) {
            return std::optional<int>{};
        }

        return res;
    }

    std::optional<int> ogl_shader_program::get_attrib_location(const std::string &name) {
        const auto res = glGetAttribLocation(program, name.c_str());

        if (res == -1) {
            return std::optional<int>{};
        }

        return res;
    }

    void *ogl_shader_program::get_metadata() {
        if (!metadata) {
            std::vector<std::uint8_t> data;
            build_metadata(program, data);

            metadata = new std::uint8_t[data.size()];
            std::copy(data.begin(), data.end(), metadata);
        }

        return metadata;
    }
}
