#pragma once

namespace eka2l1 {
    struct vec2 {
        int x, y;

        vec2(const int x, const int y)
            : x(x)
            , y(y) {}

        vec2 operator+(const vec2 &rhs) {
            return vec2(x + rhs.x, y + rhs.y);
        }

        vec2 operator-(const vec2 &rhs) {
            return vec2(x - rhs.x, y - rhs.y);
        }
    };

    struct object_size : public vec2 {
        object_size(const int x, const int y)
            : vec2(x, y) {}

        int width() {
            return x;
        }

        int height() {
            return y;
        }
    };

    using point = vec2;

    struct rect {
        vec2 top;
        object_size size;
    };
}