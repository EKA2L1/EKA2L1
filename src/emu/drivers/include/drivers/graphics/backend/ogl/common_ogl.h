#pragma once

#include <glad/glad.h>
#include <drivers/graphics/common.h>

namespace eka2l1::drivers {
    GLenum data_format_to_gl_enum(const data_format format);
}