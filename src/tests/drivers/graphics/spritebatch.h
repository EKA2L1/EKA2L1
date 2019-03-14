#pragma once

#include <drivers/graphics/texture.h>
#include <drivers/graphics/common.h>

#include <memory>

using namespace eka2l1;

class sprite_batch {
public:
    virtual void draw(const drivers::handle sprite, const eka2l1::vec2 &pos, const eka2l1::vec2 &size,
        const float rotation = 0.0f, const eka2l1::vec3 &color = eka2l1::vec3(1, 1, 1)) = 0;
};

using sprite_batch_inst = std::unique_ptr<sprite_batch>;

sprite_batch_inst create_sprite_batch(const drivers::graphic_api gr_api);
