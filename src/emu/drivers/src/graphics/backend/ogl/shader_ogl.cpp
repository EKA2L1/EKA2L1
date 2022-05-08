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
    static shader_var_type gl_enum_to_shader_var_type(const GLenum val) {
        switch (val) {
        case GL_FLOAT:
            return shader_var_type::real;

        case GL_BOOL:
            return shader_var_type::boolean;

        case GL_INT:
            return shader_var_type::integer;

        case GL_FLOAT_VEC2:
            return shader_var_type::vec2;

        case GL_FLOAT_VEC3:
            return shader_var_type::vec3;

        case GL_FLOAT_VEC4:
            return shader_var_type::vec4;

        case GL_INT_VEC2:
            return shader_var_type::ivec2;

        case GL_INT_VEC3:
            return shader_var_type::ivec3;

        case GL_INT_VEC4:
            return shader_var_type::ivec4;

        case GL_BOOL_VEC2:
            return shader_var_type::bvec2;

        case GL_SAMPLER_1D:
            return shader_var_type::sampler1d;

        case GL_SAMPLER_2D:
            return shader_var_type::sampler2d;

        case GL_SAMPLER_CUBE:
            return shader_var_type::sampler_cube;

        case GL_FLOAT_MAT2:
            return shader_var_type::mat2;

        case GL_FLOAT_MAT3:
            return shader_var_type::mat3;

        case GL_FLOAT_MAT4:
            return shader_var_type::mat4;

        default:
            break;
        }

        return shader_var_type::real;
    }

    /**
      * \brief Build a shader program's metadata. This is for client who desired performance, traded for memory.
      */
    static void build_metadata(GLuint program, std::vector<std::uint8_t> &data) {
        GLint total_attributes = 0;
        GLint total_uniforms = 0;
        GLint max_attribute_name_len = 0;
        GLint max_uniform_name_len = 0;

        glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &total_attributes);
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &total_uniforms);
        glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_attribute_name_len);
        glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_uniform_name_len);

        data.resize(16);

        reinterpret_cast<std::uint16_t *>(&data[0])[0] = 16;

        GLchar buf[257];
        GLint size = 0;
        GLsizei name_len = 0;
        GLenum type = 0;

        std::vector<std::uint16_t> offsets;

        for (int i = 0; i < total_attributes; i++) {
            glGetActiveAttrib(program, i, 256, &name_len, &size, &type, buf);
            offsets.push_back(static_cast<std::uint16_t>(data.size()));

            // Push
            data.push_back(static_cast<std::uint8_t>(name_len));
            data.insert(data.end(), buf, buf + name_len);

            buf[name_len] = '\0';

            std::int32_t location = glGetAttribLocation(program, buf);
            shader_var_type var_type = gl_enum_to_shader_var_type(type);

            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&location), reinterpret_cast<std::uint8_t *>(&location) + sizeof(location));
            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&var_type), reinterpret_cast<std::uint8_t*>(&var_type + 1));            
            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&size), reinterpret_cast<std::uint8_t *>(&size + 1));
        }

        for (int i = 0; i < total_attributes; i++) {
            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&offsets[i]), reinterpret_cast<std::uint8_t *>(&offsets[i] + 1));
        }

        reinterpret_cast<std::uint16_t *>(&data[0])[1] = static_cast<std::uint16_t>(data.size());
        reinterpret_cast<std::uint16_t *>(&data[0])[2] = static_cast<std::uint16_t>(total_attributes);
        reinterpret_cast<std::uint16_t *>(&data[0])[3] = static_cast<std::uint16_t>(total_uniforms);
        reinterpret_cast<std::uint16_t *>(&data[0])[4] = static_cast<std::uint16_t>(max_attribute_name_len);
        reinterpret_cast<std::uint16_t *>(&data[0])[5] = static_cast<std::uint16_t>(max_uniform_name_len);

        offsets.clear();

        for (int i = 0; i < total_uniforms; i++) {
            glGetActiveUniform(program, i, 256, &name_len, &size, &type, buf);

            buf[name_len] = '\0';

            // Push
            offsets.push_back(static_cast<std::uint16_t>(data.size()));
            data.push_back(static_cast<std::uint8_t>(name_len));
            data.insert(data.end(), buf, buf + name_len);

            std::int32_t location = glGetUniformLocation(program, buf);
            shader_var_type var_type = gl_enum_to_shader_var_type(type);

            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&location), reinterpret_cast<std::uint8_t *>(&location) + sizeof(location));
            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&var_type), reinterpret_cast<std::uint8_t*>(&var_type + 1));
            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&size), reinterpret_cast<std::uint8_t *>(&size + 1));
        }
        
        for (int i = 0; i < total_uniforms; i++) {
            data.insert(data.end(), reinterpret_cast<std::uint8_t *>(&offsets[i]), reinterpret_cast<std::uint8_t *>(&offsets[i] + 1));
        }
        
        reinterpret_cast<std::uint16_t *>(&data[0])[6] = static_cast<std::uint16_t>(data.size());
        reinterpret_cast<std::uint16_t *>(&data[0])[7] = 0;     // Reserved
    }

    ogl_shader_module::ogl_shader_module()
        : shader(0) {
    }

    ogl_shader_module::ogl_shader_module(const std::string &path, const shader_module_type type, const std::string &extra_header)
        : shader(0) {
        common::ro_std_file_stream stream(path, std::ios_base::binary);
        if (!stream.valid()) {
            LOG_ERROR(DRIVER_GRAPHICS, "Shader file stream with path {} is invalid!", path);
        }
        
        std::string whole_code;
        whole_code.resize(stream.size());

        stream.read(whole_code.data(), whole_code.size());
        whole_code.insert(whole_code.begin(), extra_header.begin(), extra_header.end());

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

    bool ogl_shader_module::create(graphics_driver *driver, const char *data, const std::size_t size, const shader_module_type type, std::string *compile_log) {
        shader = glCreateShader(((type == shader_module_type::vertex) ? GL_VERTEX_SHADER :
            ((type == shader_module_type::fragment) ? GL_FRAGMENT_SHADER : GL_GEOMETRY_SHADER)));

        glShaderSource(shader, 1, &data, nullptr);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        if (compile_log) {
            compile_log->resize(log_length);
            glGetShaderInfoLog(shader, log_length, nullptr, compile_log->data());
        }

        if (!success) {
            if (!compile_log) {
                std::string error_log(log_length, '\0');
                glGetShaderInfoLog(shader, log_length, nullptr, error_log.data());

                LOG_ERROR(DRIVER_GRAPHICS, "Error while compiling shader: {}, abort", error_log);
            }

            glDeleteShader(shader);
            shader = 0;

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

    bool ogl_shader_program::create(graphics_driver *driver, shader_module *vertex_module, shader_module *fragment_module, std::string *link_log) {
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

        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

        if (link_log) {
            link_log->resize(log_length);
            glGetProgramInfoLog(program, log_length, nullptr, link_log->data());
        }

        if (!success) {
            if (!link_log) {
                std::string error(log_length, '\0');

                glGetProgramInfoLog(program, log_length, nullptr, error.data());
                LOG_ERROR(DRIVER_GRAPHICS, "Error while linking shader program: {}", error);
            }

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
