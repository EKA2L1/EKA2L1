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

        create(vertex_code.data(), vertex_code.size(), fragment_code.data(),
            fragment_code.size());
    }

    ogl_shader::ogl_shader(const char *vert_data, const std::size_t vert_size,
        const char *frag_data, const std::size_t frag_size) {
        create(vert_data, vert_size, frag_data, frag_size);
    }

    bool ogl_shader::create(const char *vert_data, const std::size_t vert_size,
        const char *frag_data, const std::size_t frag_size) {
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

    bool ogl_shader::use() {
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
    
    bool ogl_shader::set(const std::string &name, const shader_set_var_type var_type, const void *data) {
        const auto loc = glGetUniformLocation(program, name.c_str());

        if (loc == -1) {
            return false;
        }

        switch (var_type) {
        case shader_set_var_type::integer: {
            glUniform1i(loc, *reinterpret_cast<const GLint*>(data));
            return true;
        }

        case shader_set_var_type::mat4: {
            glUniformMatrix4fv(loc, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(data));
            return true;
        }

        case shader_set_var_type::vec3: {
            glUniform3fv(loc, 1, reinterpret_cast<const GLfloat*>(data));
            return true;
        }

        case shader_set_var_type::vec4: {
            glUniform4fv(loc, 1, reinterpret_cast<const GLfloat*>(data));
            return true;
        }

        default:
            break;
        }

        return false;
    }
}
