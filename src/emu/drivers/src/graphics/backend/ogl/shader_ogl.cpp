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

            data.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(glGetAttribLocation(program, buf))));
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

            data.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(glGetUniformLocation(program, buf))));
        }
    }

    ogl_shader::ogl_shader(const std::string &vert_path,
        const std::string &frag_path) {
        std::ifstream vert_file(vert_path, std::ios_base::binary);
        std::ifstream frag_file(frag_path, std::ios_base::binary);

        std::stringstream vertex_stream, fragment_stream;

        vertex_stream << vert_file.rdbuf();
        fragment_stream << frag_file.rdbuf();

        vert_file.close();
        frag_file.close();

        std::string vertex_code = vertex_stream.str();
        std::string fragment_code = fragment_stream.str();

        create(nullptr, vertex_code.data(), vertex_code.size(), fragment_code.data(),
            fragment_code.size());
    }

    ogl_shader::ogl_shader(const char *vert_data, const std::size_t vert_size,
        const char *frag_data, const std::size_t frag_size) {
        create(nullptr, vert_data, vert_size, frag_data, frag_size);
    }

    ogl_shader::~ogl_shader() {
        if (metadata) {
            delete metadata;
        }
    }

    bool ogl_shader::create(graphics_driver *driver, const char *vert_data, const std::size_t vert_size,
        const char *frag_data, const std::size_t frag_size) {
        metadata = nullptr;

        std::uint32_t vert = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vert, 1, &vert_data, nullptr);
        glCompileShader(vert);

        int success;
        char error[512];

        glGetShaderiv(vert, GL_COMPILE_STATUS, &success);

        if (!success) {
            glGetShaderInfoLog(vert, 512, nullptr, error);
            LOG_ERROR("Error while compiling vertex shader: {}, abort", error);

            glDeleteShader(vert);
            return false;
        }

        std::uint32_t frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(frag, 1, &frag_data, nullptr);
        glCompileShader(frag);

        glGetShaderiv(frag, GL_COMPILE_STATUS, &success);

        if (!success) {
            glGetShaderInfoLog(frag, 512, nullptr, error);
            LOG_ERROR("Error while compiling fragment shader: {}, abort", error);

            glDeleteShader(vert);
            glDeleteShader(frag);

            return false;
        }

        program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);

        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &success);

        if (!success) {
            glGetProgramInfoLog(program, 512, nullptr, error);
            LOG_ERROR("Error while linking shader program: {}", error);
        }

        glDeleteShader(vert);
        glDeleteShader(frag);

        if (!success) {
            return false;
        }

        return true;
    }

    bool ogl_shader::use(graphics_driver *driver) {
        glUseProgram(program);
        return true;
    }

    std::optional<int> ogl_shader::get_uniform_location(const std::string &name) {
        const auto res = glGetUniformLocation(program, name.c_str());

        if (res == -1) {
            return std::optional<int>{};
        }

        return res;
    }

    std::optional<int> ogl_shader::get_attrib_location(const std::string &name) {
        const auto res = glGetAttribLocation(program, name.c_str());

        if (res == -1) {
            return std::optional<int>{};
        }

        return res;
    }

    bool ogl_shader::set(graphics_driver *driver, const std::string &name, const shader_set_var_type var_type, const void *data) {
        const auto loc = glGetUniformLocation(program, name.c_str());

        if (loc == -1) {
            return false;
        }

        return set(driver, loc, var_type, data);
    }

    bool ogl_shader::set(graphics_driver *driver, const int binding, const shader_set_var_type var_type, const void *data) {
        switch (var_type) {
        case shader_set_var_type::integer: {
            glUniform1i(binding, *reinterpret_cast<const GLint *>(data));
            return true;
        }

        case shader_set_var_type::mat4: {
            glUniformMatrix4fv(binding, 1, GL_FALSE, reinterpret_cast<const GLfloat *>(data));
            return true;
        }

        case shader_set_var_type::vec3: {
            glUniform3fv(binding, 1, reinterpret_cast<const GLfloat *>(data));
            return true;
        }

        case shader_set_var_type::vec4: {
            glUniform4fv(binding, 1, reinterpret_cast<const GLfloat *>(data));
            return true;
        }

        default:
            break;
        }

        return false;
    }

    void *ogl_shader::get_metadata() {
        if (!metadata) {
            std::vector<std::uint8_t> data;
            build_metadata(program, data);

            metadata = new std::uint8_t[data.size()];
            std::copy(data.begin(), data.end(), metadata);
        }

        return metadata;
    }
}
