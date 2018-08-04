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
#pragma once

namespace eka2l1 {
	/*! \brief A 2D Vector */
    struct vec2 {
        int x, y;

        vec2() {}

        vec2(const int x, const int y)
            : x(x)
            , y(y) {}

        vec2 operator+(const vec2 &rhs) {
            return vec2(x + rhs.x, y + rhs.y);
        }

        vec2 operator-(const vec2 &rhs) {
            return vec2(x - rhs.x, y - rhs.y);
        }

        vec2 operator*(const int rhs) {
            return vec2(x * rhs, y * rhs);
        }
    };

    struct object_size : public vec2 {
        object_size() : vec2() {}

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
