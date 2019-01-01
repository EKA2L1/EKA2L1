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

#include <cstdint>
#include <cmath>
#include <array>
#include <initializer_list>
#include <numeric>
#include <vector>

namespace eka2l1 {
    /*! \brief A basic template type vector 
    */
    template <typename T, int SIZE>
    struct vecx {
        std::vector<T> elements;

    public:
        T &operator[](const std::size_t idx) {
            return elements[idx];
        }

        vecx() {
            elements.resize(SIZE);
        }

        // Allow non-explicit
        vecx(const std::initializer_list<T> list)
            : elements(list) {
        }

        T normalize() {
            return static_cast<T>(std::sqrt(std::accumulate(elements.begin(), elements.end(), 0, 
                [](const T &lhs, const T &rhs) {
                    return lhs + rhs * rhs;
                })));
        }
    };

    using vec2d = vecx<double, 2>;

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

        bool operator == (const vec2 &rhs) {
            return (x == rhs.x) && (y == rhs.y);
        }

        void operator = (const vec2 &rhs) {
            x = rhs.x;
            y = rhs.y;
        }
    };

    struct vec3: public vec2 {
        int z;

        vec3() {}

        vec3(const int x, const int y, const int z)
            : vec2(x, y), z(z) {}

        vec3 operator+(const vec3 &rhs) {
            return vec3(x + rhs.x, y + rhs.y, z + rhs.z);
        }

        vec3 operator-(const vec3 &rhs) {
            return vec3(x - rhs.x, y - rhs.y, z -rhs.z);
        }

        vec3 operator*(const int rhs) {
            return vec3(x * rhs, y * rhs, z * rhs);
        }
    };

    struct object_size : public vec2 {
        object_size() : vec2() {}

        object_size(const vec2 &v)
            : vec2(v) {

        }

        object_size(const int x, const int y)
            : vec2(x, y) {}

        int width() const {
            return x;
        }

        int height() const {
            return y;
        }
        
        void operator = (const vec2 &rhs) {
            x = rhs.x;
            y = rhs.y;
        }
    };

    using point = vec2;

    struct rect {
        vec2 top;
        object_size size;
    };
}
