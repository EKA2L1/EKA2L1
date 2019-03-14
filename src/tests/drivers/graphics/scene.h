#pragma once

#include "spritebatch.h"

#include <drivers/graphics/common.h>
#include <drivers/graphics/graphics.h>

#include <common/bitmap.h>

using namespace eka2l1;

enum scene_flags {
    init = 0x1
};

struct scene {
    std::uint32_t         flags {0};
};

struct bitmap_draw_scene: public scene {
    common::bmp_header    bmp_header;
    common::dib_header_v1 dib_header;

    drivers::handle tex;
};

struct scenes {
    drivers::graphic_api gr_api;
    drivers::graphics_driver_ptr driver;
    sprite_batch_inst spritebatch;

    bitmap_draw_scene bmp_draw_scene;
};
