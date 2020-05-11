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

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <vector>

namespace eka2l1 {
    /*! \brief A basic template type vector 
    */
    template <typename T, int SIZE>
    struct vecx {
        mutable std::vector<T> elements;

    public:
        T &operator[](const std::size_t idx) const {
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

        vecx<T, SIZE> operator*(const int num) {
            vecx<T, SIZE> newv;
            for (std::size_t i = 0; i < SIZE; i++) {
                newv[i] = elements[i] * num;
            }

            return newv;
        }

        bool operator==(const eka2l1::vecx<T, SIZE> &v) {
            for (std::size_t i = 0; i < SIZE; i++) {
                if (elements[i] != v.elements[i]) {
                    return false;
                }
            }

            return true;
        }

        bool operator!=(const eka2l1::vecx<T, SIZE> &v) {
            return !(*this == v);
        }
    };

    using vec2d = vecx<double, 2>;

    /*! \brief A 2D Vector */
    struct vec2 {
        int x, y;

        vec2() {}

        vec2(const int d)
            : x(d)
            , y(d) {
        }

        vec2(const int x, const int y)
            : x(x)
            , y(y) {}

        vec2 operator+(const vec2 &rhs) const {
            return vec2(x + rhs.x, y + rhs.y);
        }

        vec2 operator-(const vec2 &rhs) const {
            return vec2(x - rhs.x, y - rhs.y);
        }

        vec2 operator*(const int rhs) const {
            return vec2(x * rhs, y * rhs);
        }

        bool operator==(const vec2 &rhs) const {
            return (x == rhs.x) && (y == rhs.y);
        }

        bool operator!=(const vec2 &rhs) const {
            return (x != rhs.x) || (y != rhs.y);
        }

        bool operator<(const vec2 &rhs) const {
            return (x < rhs.x && y < rhs.y);
        }

        void operator+=(const vec2 &rhs) {
            x += rhs.x;
            y += rhs.y;
        }

        void operator-=(const vec2 &rhs) {
            x -= rhs.x;
            y -= rhs.y;
        }

        void operator=(const vec2 &rhs) {
            x = rhs.x;
            y = rhs.y;
        }

        vec2 abs() {
            return { x < 0 ? -x : x, y < 0 ? -y : y };
        }
    };

    struct vec3 : public vec2 {
        int z;

        vec3() {}

        vec3(const int x, const int y, const int z)
            : vec2(x, y)
            , z(z) {}

        vec3 operator+(const vec3 &rhs) const {
            return vec3(x + rhs.x, y + rhs.y, z + rhs.z);
        }

        vec3 operator-(const vec3 &rhs) const {
            return vec3(x - rhs.x, y - rhs.y, z - rhs.z);
        }

        vec3 operator*(const int rhs) const {
            return vec3(x * rhs, y * rhs, z * rhs);
        }
    };

    struct object_size : public vec2 {
        object_size()
            : vec2() {}

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

        void operator=(const vec2 &rhs) {
            x = rhs.x;
            y = rhs.y;
        }
    };

    using point = vec2;

    /**
     * \brief A simple structure represents a rectangle.
     * 
     * This struct describes the rectangle by the position of the top left of the rectangle,
     * and the size of the rectangle.
     * 
     * This rectangle is in 2D.
     */
    struct rect {
        vec2 top; ///< Top left of the rectangle.
        object_size size; ///< Size of the rectangle.

        rect()
            : top(0, 0)
            , size(0, 0) {
        }

        explicit rect(const vec2 &top_, const vec2 &obj_size_)
            : top(top_)
            , size(obj_size_) {
        }

        /**
         * \brief Check if the rectangle region is empty.
         * 
         * This is equals to checking if the size is 0
         */
        bool empty() const {
            return (size.x == 0) && (size.y == 0);
        }

        bool valid() const {
            return (size.x > 0) && (size.y > 0) && (top.x >= 0) && (top.y >= 0);
        }

        bool contains(const eka2l1::vec2 point) {
            if ((top.x <= point.x) && (top.y <= point.y) && (top.x + size.x >= point.x) && (top.y + size.y >= point.y))
                return true;

            return false;
        }

        bool contains(const eka2l1::rect rect) {
            return (size.x >= rect.size.x) && (size.y >= rect.size.y)
                && (top.y >= rect.top.x) && (top.y >= rect.top.y);
        }

        void merge(eka2l1::rect other) {
            if (contains(other)) {
                return;
            }

            const int newx = std::min<int>(top.x, other.top.x);
            const int newy = std::min<int>(top.y, other.top.y);

            size.x = std::max<int>(top.x + size.x, other.top.x + other.size.x) - newx;
            size.y = std::max<int>(top.y + size.y, other.top.y + other.size.y) - newy;
            top.x = newx;
            top.y = newy;
        }

        void transform_from_symbian_rectangle() {
            // Symbian TRect has the second vector as the bottom right
            size = size - top;
        }
    };
}
