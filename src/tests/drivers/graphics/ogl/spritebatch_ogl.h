#include "../spritebatch.h"

#include <drivers/graphics/backend/ogl/shader_ogl.h>
#include <drivers/graphics/backend/ogl/texture_ogl.h>

#include <glad/glad.h>

using namespace eka2l1;

class sprite_batch_ogl: public sprite_batch {
    drivers::ogl_shader shader_;
    GLuint quad_vao;

public:
    sprite_batch_ogl();
    void draw(const drivers::handle sprite, const eka2l1::vec2 &pos, const eka2l1::vec2 &size,
        const float rotation = 0.0f, const eka2l1::vec3 &color = eka2l1::vec3(1, 1, 1)) override;
};
