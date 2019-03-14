/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/

#include "spritebatch_ogl.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const char *batch_vert_shader =  "#version 330 core\n"
                            "layout (location = 0) in vec4 vertex;\n"
                            "out vec2 TexCoords;\n"
                            "uniform mat4 model;\n"
                            "uniform mat4 projection;\n"
                            "void main()\n"
                            "{\n"
                            "   TexCoords = vertex.zw;\n"
                            "   gl_Position = projection * model * vec4(vertex.xy, 0.0, 1.0);\n"
                            "}\n";

const char *batch_frag_shader = "#version 330 core\n"
                                "in vec2 TexCoords;\n"
                                "out vec4 color;\n"
                                "uniform sampler2D image;\n"
                                "uniform vec3 spriteColor;\n"
                                "void main()\n"
                                "{\n"    
                                "    color = vec4(spriteColor, 1.0) * texture(image, TexCoords);\n"
                                "}\n";

sprite_batch_ogl::sprite_batch_ogl() {
    shader_.create(batch_vert_shader, 0, batch_frag_shader, 0);

    GLuint vbo;
    GLfloat vertices[] = { 
        // Pos      // Tex
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 

        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &quad_vao);
    glGenBuffers(1, &vbo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(quad_vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);  
    glBindVertexArray(0);
}

void sprite_batch_ogl::draw(const drivers::handle sprite, const eka2l1::vec2 &pos, const eka2l1::vec2 &size, const float rotation, const eka2l1::vec3 &color) {
    shader_.use();

    glm::mat4 model;
    model = glm::translate(model, glm::vec3(pos.x, pos.y, 0.0f));  

    model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.0f)); 
    model = glm::rotate(model, rotation, glm::vec3(0.0f, 0.0f, 1.0f)); 
    model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f));

    model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f)); 
  
    shader_.set("model", drivers::shader_set_var_type::mat4, glm::value_ptr(model));
    shader_.set("spriteColor", drivers::shader_set_var_type::vec3, &color);
  
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(sprite));

    glBindVertexArray(quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
