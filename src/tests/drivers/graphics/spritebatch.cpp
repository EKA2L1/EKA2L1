#include "spritebatch.h"
#include "ogl/spritebatch_ogl.h"

sprite_batch_inst create_sprite_batch(const drivers::graphic_api gr_api) {
    switch (gr_api) {
    case drivers::graphic_api::opengl: {
        return std::make_unique<sprite_batch_ogl>();
    }

    default:
        break;
    }

    return nullptr;
}
