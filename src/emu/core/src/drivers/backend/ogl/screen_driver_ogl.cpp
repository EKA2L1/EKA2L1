#include <drivers/backend/screen_driver_gl.h>
#include <common/log.h>
#include <drivers/emu_window.h>
#include <glad/glad.h>

namespace eka2l1 {
    namespace driver {
        const char* fb_render_vertex = "#version 330 core\n"
            "layout (location = 0) in vec2 aPos;\n"
            "layout (location = 1) in vec2 aTexCoords;\n"
            "out vec2 oTexCoords;\n"
            "void main() {\n"
            "gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);\n"
            "oTexCoords = aTexCoords;\n"
            "}\0";

        const char* fb_render_frag = "#version 330 core\n"
            "out vec4 oFragColor;\n"
            "in vec2 iTexCoords;\n"
            "uniform sampler2D fb;\n"
            "void main() {\n"
            "oFragColor = texture(fb, iTexCoords);\n"
            "}\0";

        const char* console_shader_vert = "#version 330 core\n"
            "layout (location = 0) in vec4 vertex;\n"
            "out vec2 texCoords;\n"
            "uniform mat4 proj;\n"
            "void main() {\n"
            "gl_Position = proj * vec4(vertex.xy, 0.0, 1.0);\n"
            "texCoords = vertex.zw;\n"
            "}";

        const char* console_shader_frag = "#version 330 core\n"
            "in vec2 texCoords;\n"
            "out vec4 color;\n"
            "uniform sampler2D text;\n"
            "uniform vec3 textColor;\n"
            "void main() {\n"
            "vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, texCoords).r);\n"
            "color = vec4(textColor, 1.0) * sampled;\n"
            "}";

        float quad_verts[] = { 
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
            1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
            1.0f, -1.0f,  1.0f, 0.0f,
            1.0f,  1.0f,  1.0f, 1.0f
        };

        void screen_driver_ogl::init_font() {
            if (FT_Init_FreeType(&ft)) {
                LOG_ERROR("Can't initalize Freetype!");
                return;
            }

            if (FT_New_Face(ft, "comic.ttf", 0, &face)) {
                LOG_ERROR("Can't load Comic Sans (No Undertale here!)");
                return;
            }

            FT_Set_Pixel_Sizes(face, 0, 48);
            
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            for (uint8_t i = 0; i < 128; i++) {
                if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
                    LOG_ERROR("Loading char: {} fail!", i);
                }

                GLuint tex;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width,
                    face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
                    face->glyph->bitmap.buffer);


                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
                
                ttf_char tchar = {
                    tex, 
                    face->glyph->advance.x,
                    vec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                    vec2(face->glyph->bitmap_left, face->glyph->bitmap_top)
                };

                ttf_chars.emplace(i, tchar);
            }

            FT_Done_Face(face);
            FT_Done_FreeType(ft);
        }

        void screen_driver_ogl::init(emu_window_ptr win, object_size &screen_size, object_size &font_size) {
            ssize = screen_size;
            fsize = font_size;

            emu_win = win;

            emu_win->make_current();

            int res = gladLoadGL();

            glViewport(0, 0, screen_size.width(), screen_size.height());

            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            glGenTextures(1, &fbt);
            glBindTexture(GL_TEXTURE_2D, fbt);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screen_size.width(), screen_size.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbt, 0);

            glGenRenderbuffers(1, &rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screen_size.width(), screen_size.height());
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
           
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                LOG_INFO("Framebuffer not complete!");
            }
            // unbind our buffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Make quad vao, vbo
            glGenVertexArrays(1, &quad_vao);
            glGenBuffers(1, &quad_vbo);
            glBindVertexArray(quad_vao);
            glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), &quad_verts, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

            auto vert = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vert, 1, &fb_render_vertex, nullptr);
            glCompileShader(vert);

            int  success;
            char infoLog[512];
            glGetShaderiv(vert, GL_COMPILE_STATUS, &success);

            if (!success) {
                glGetShaderInfoLog(vert, 512, NULL, infoLog);
                LOG_ERROR("Shader compile error: {}", infoLog);
            }

            auto frag = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(frag, 1, &fb_render_frag, nullptr);
            glCompileShader(frag);

            glGetShaderiv(frag, GL_COMPILE_STATUS, &success);

            if (!success) {
                glGetShaderInfoLog(frag, 512, NULL, infoLog);
                LOG_ERROR("Shader compile error: {}", infoLog);
            }

            render_program = glCreateProgram();
            glAttachShader(render_program, vert);
            glAttachShader(render_program, frag);

            glLinkProgram(render_program);

            glDeleteShader(vert);
            glDeleteShader(frag);

            vert = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vert, 1, &console_shader_vert, nullptr);
            glCompileShader(vert);

            if (!success) {
                glGetShaderInfoLog(vert, 512, NULL, infoLog);
                LOG_ERROR("Shader compile error: {}", infoLog);
            }

            frag = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(frag, 1, &console_shader_frag, nullptr);
            glCompileShader(frag);

            glGetShaderiv(frag, GL_COMPILE_STATUS, &success);

            if (!success) {
                glGetShaderInfoLog(frag, 512, NULL, infoLog);
                LOG_ERROR("Shader compile error: {}", infoLog);
            }

            font_program = glCreateProgram();
            glAttachShader(font_program, vert);
            glAttachShader(font_program, frag);

            glLinkProgram(font_program);

            glDeleteShader(vert);
            glDeleteShader(frag);

            ortho = glm::ortho(0.0f, static_cast<GLfloat>(ssize.width()), 0.0f, static_cast<GLfloat>(ssize.height()));
            
            glUseProgram(render_program);

            uint32_t loc = glGetUniformLocation(render_program, "fb");

            //glUniform1i(loc, 0);

            loc = glGetUniformLocation(font_program, "proj");

            glUseProgram(font_program);
            glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(ortho));

            glGenVertexArrays(1, &dynquad_vao);
            glGenBuffers(1, &dynquad_vbo);
            glBindVertexArray(dynquad_vao);
            glBindBuffer(GL_ARRAY_BUFFER, dynquad_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            init_font();
        }

        void screen_driver_ogl::shutdown() {
            glDeleteVertexArrays(1, &dynquad_vao);
            glDeleteVertexArrays(1, &quad_vao);
            glDeleteBuffers(1, &dynquad_vbo);
            glDeleteBuffers(1, &quad_vbo);

            glDeleteProgram(font_program);
            glDeleteProgram(render_program);

            glDeleteTextures(1, &fbt);
            glDeleteRenderbuffers(1, &rbo);
            glDeleteFramebuffers(1, &fbo);

            for (auto& ttfc: ttf_chars) {
                glDeleteTextures(1, &ttfc.second.tex_id);
            }

            emu_win->done_current();
        }

        void screen_driver_ogl::blit(const std::string &text, const point &where) {
            glUseProgram(font_program);
            glUniform3f(glGetUniformLocation(font_program, "textColor"), 0.1f, 0.1f, 0.1f);
            glActiveTexture(GL_TEXTURE0);
            glBindVertexArray(dynquad_vao);

            GLfloat xx = where.x;

            for (const auto& c : text) {
                ttf_char glc = ttf_chars[c];

                GLfloat x = xx + glc.bearing.x;
                GLfloat y = where.y - (glc.size.y - glc.bearing.y);

                GLfloat w = glc.size.x;
                GLfloat h = glc.size.y;

                GLfloat vertices[6][4] = {
                    { x, y + h, 0.0, 0.0 },
                    { x, y, 0.0, 1.0},
                    { x + w, y, 1.0, 1.0},

                    { x, y + h, 0.0, 0.0 },
                    { x + w, y, 1.0, 1.0 },
                    { x + w, y + h, 1.0, 0.0 }
                };

                glBindTexture(GL_TEXTURE_2D, glc.tex_id);
                glBindBuffer(GL_ARRAY_BUFFER, dynquad_vbo);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                xx += (glc.advance >> 6);
            }

            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        bool screen_driver_ogl::scroll_up(rect &trect) {
            return true;
        }

        void screen_driver_ogl::clear(rect &trect) {
            
        }

        // These operations should never been used
        void screen_driver_ogl::set_pixel(const point &pos, uint8_t color) {
            glRasterPos2i(pos.x, pos.y);
            glDrawPixels(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &color);
        }

        int screen_driver_ogl::get_pixel(const point &pos) {
            uint8_t pixel = 0;
            glReadPixels(pos.x, pos.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

            return pixel;
        }

        void screen_driver_ogl::set_word(const point &pos, int word) {
            glRasterPos2i(pos.x, pos.y);
            glDrawPixels(1, 1, GL_RGBA, GL_UNSIGNED_SHORT, &word);  // Unsure if it's color or rgba
        }

        int screen_driver_ogl::get_word(const point &pos) {
            int word;
            glReadPixels(pos.x, pos.y, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, &word);

            return word;
        }

        void screen_driver_ogl::set_line(const point &pos, const pixel_line &pl) {
            glRasterPos2i(pos.x, pos.y);
            glDrawPixels(pl.size(), 1, GL_RGBA, GL_UNSIGNED_SHORT, pl.data());  // Unsure if it's color or rgba
        }

        void screen_driver_ogl::get_line(const point &pos, pixel_line &pl) {
            glReadPixels(pos.x, pos.y, pl.size(), 1, GL_RGBA, GL_UNSIGNED_SHORT, pl.data());
        }

        void screen_driver_ogl::begin_render() {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glEnable(GL_DEPTH_TEST);

            glClearColor(0.5f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glEnable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        void screen_driver_ogl::end_render() {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDisable(GL_DEPTH_TEST);

            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(render_program);
            glBindVertexArray(quad_vao);
            glBindTexture(GL_TEXTURE_2D, fbt);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            emu_win->swap_buffer();
            emu_win->poll_events();
        }
    }
}