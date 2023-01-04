/*
 * gnuVG - a free Vector Graphics library
 * Copyright (C) 2014 by Anton Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * This file is partially derivate work. The following is the copyright notice
 * of the original contents. (This pertains only to the original work, the derivate
 * work is licensed using LGPL version 3 or later.)
 *
 * Copyright (C) 2004, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __GNUVG_MATRIX_HH
#define __GNUVG_MATRIX_HH

#include <dispatch/libraries/vg/consts.h>
#include <dispatch/libraries/vg/gnuVG_error.hh>

#include <math.h>
#include <string>

#define GNUVG_CLAMP(v,min,max) ((v < min) ? min : ((v > max) ? max : v))
#define GNUVG_DEG_TO_RAD(d) (d * (M_PI / 180.0f))

namespace gnuVG {

#define TRAP_NAN_ERROR
#ifdef TRAP_NAN_ERROR
	class CoordinateIsNaN {};
#endif

	static inline VGfloat kappa() {
		return 4.0f * (sqrtf(2.0f) - 1) / 3.0f;
	}

	// Linearly interpolate between a and b, based on t.
	// If t is 0, return a; if t is 1, return b; else interpolate.
	// t must be [0..1].
	static inline float interpolate(float a, float b, float t) {
		return a + (b - a) * t;
	}

	static inline VGfloat sign(VGfloat value) {
		return value < 0.0f ? -1.0f : 1.0f;
	}

	class Point {
	public:
		union {
			struct {
				VGfloat x, y;
			};
			VGfloat c[2];
		};

		Point() : x(0.0f), y(0.0f) {}
		Point(VGfloat _x, VGfloat _y) : x(_x), y(_y) {
#ifdef TRAP_NAN_ERROR
			if(isnan(x) || isnan(y))
				throw CoordinateIsNaN();
#endif
		}
		Point(const Point &p) : x(p.x), y(p.y) {
#ifdef TRAP_NAN_ERROR
			if(isnan(x) || isnan(y))
				throw CoordinateIsNaN();
#endif
		}

		void set(VGfloat _x, VGfloat _y) {
			x = _x;
			y = _y;
#ifdef TRAP_NAN_ERROR
			if(isnan(x) || isnan(y))
				throw CoordinateIsNaN();
#endif
		}
		inline float lengthSquared() const {
			return x * x + y * y;
		}
		inline float length() const {
			return sqrtf(lengthSquared());
		};

		inline float cross(const Point& a) const {
			return x * a.y - y * a.x;
		}

		inline float dot(const Point& a) const {
			return x * a.x + y * a.y;
		}

		inline void normalize() {
			float tempLength = length();

			if (tempLength) {
				x /= tempLength;
				y /= tempLength;
			}
		}

		inline Point normal () const {
			Point retval;
			auto len = length();
			retval.x = -y/len;
			retval.y = x/len;
			return retval;
		}

		inline VGfloat angle_with(const Point& other) {
			return atan2f(cross(other), dot(other));
		}

		inline void subtract(const Point &other) {
			x -= other.x;
			y -= other.y;
		}

		inline VGfloat angle_between(Point U, Point V) const {
			U.subtract(*this);
			V.subtract(*this);
			auto cross = U.cross(V);
			auto dot = U.dot(V);
			return atan2(cross, dot);
		}

		// rotate this point around origo
		void rotate(Point origo, VGfloat angle) {
			VGfloat nx = x - origo.x;
			VGfloat ny = y - origo.y;

			VGfloat mx = nx * cosf(angle) - ny * sinf(angle);
			VGfloat my = nx * sinf(angle) + ny * cosf(angle);

			x = origo.x + mx;
			y = origo.y + my;
		}
	};

	class Matrix {
	private:
	public:
		/*****************
		 * pointInPreviousSpace = Matrix * pointInNewSpace
		 *
		 *               | a d g |   | Xn |
		 * (Xp, Yp, 1) = | b e h | * | Yn |
		 *               | c f i |   | 1  |
		 *
		 *****************/
		VGfloat a, d, g,
			b, e, h,
			c, f, i;

		inline void print_matrix(const std::string &topic) {
			GNUVG_ERROR("---- mat for %s\n"
				    "     m = [%f, %f, %f;\\\n"
				    "          %f, %f, %f;\\\n"
				    "          %f, %f, %f]\n",
				    topic.c_str(),
				    a, d, g,
				    b, e, h,
				    c, f, i);
		}

		Matrix();
		Matrix(
			VGfloat _a, VGfloat _d, VGfloat _g,
			VGfloat _b, VGfloat _e, VGfloat _h,
			VGfloat _c, VGfloat _f, VGfloat _i
			) :
			a(_a), d(_d), g(_g),
			b(_b), e(_e), h(_h),
			c(_c), f(_f), i(_i) {}

		void init_identity();
		void translate(VGfloat x, VGfloat y);
		void scale(VGfloat x, VGfloat y);
		void shear(VGfloat shx, VGfloat shy);
		void rotate(VGfloat angle_in_radians);
		void multiply(const Matrix &A, const Matrix &B);

		void set_to(const Matrix &src) {
			a = src.a; d = src.d; g = src.g;
			b = src.b; e = src.e; h = src.h;
			c = src.c; f = src.f; i = src.i;
		}

		bool isAffine () const {
			if(c == 0.0f && f == 0.0f && i == 1.0f)
				return true;
			return false;
		}

		void get(VGfloat* matrix) {
			matrix[0] = a;
			matrix[1] = b;
			matrix[2] = c;
			matrix[3] = d;
			matrix[4] = e;
			matrix[5] = f;
			matrix[6] = g;
			matrix[7] = h;
			matrix[8] = i;
		}

		bool invert() {
			bool affine = isAffine();
			VGfloat det00 = e * i - f * h;
			VGfloat det01 = c * h - b * i;
			VGfloat det02 = b * f - c * e;

			VGfloat det = a * det00 + d * det01 + g * det02;
			if( det == 0.0f ) return false;	//singular, leave the matrix unmodified and return false
			det = 1.0f / det;

			Matrix t;
			t.a = det * det00;
			t.b = det * det01;
			t.c = det * det02;
			t.d = det * (f * g - d * i);
			t.e = det * (a * i - c * g);
			t.f = det * (c * d - a * f);
			t.g = det * (d * h - e * g);
			t.h = det * (b * g - a * h);
			t.i = det * (a * e - b * d);
			if(affine) {
				//affine matrix stays affine
				t.c = 0.0f;
				t.f = 0.0f;
				t.i = 1.0f;
			}
			set_to(t);

			return true;
		}

		Point map_point(const Point &p);
	};

	class Triangle {
	public:
		Point v[3];

		void set(const Point &p1, const Point &p2, const Point &p3) {
			v[0] = p1;
			v[1] = p2;
			v[2] = p3;
		}

		Triangle(const Point &p1, const Point &p2, const Point &p3) {
			set(p1, p2, p3);
		}

		Triangle() {
			Point p1, p2, p3;
			set(p1, p2, p3);
		}

		bool contains_point(const Point &p);
		bool overlap_triangle(const Triangle &other);
	};

	class Point3D {
	public:
		VGfloat x, y, z;

		Point3D() : x(0.0f), y(0.0f), z(0.0f) { /* empty constructor */ }
		Point3D(VGfloat _x, VGfloat _y, VGfloat _z) : x(_x), y(_y), z(_z) { /* empty constructor */ }

		float dot(const Point3D& a) const {
			return x * a.x + y * a.y + z * a.z;
		}

		void cross(const Point3D& a, const Point3D& b) {
			float t_x = a.y * b.z - a.z * b.y;
			float t_y = a.z * b.x - a.x * b.z;
			float t_z = a.x * b.y - a.y * b.x;
			x = t_x;
			y = t_y;
			z = t_z;
		}

		Point3D cross(const Point3D& point) const {
			Point3D result;
			result.cross(*this, point);
			return result;
		}

		float lengthSquared() const { return this->dot(*this); }
		float length() const { return sqrtf(lengthSquared()); }

		void normalize() {
			float tempLength = length();

			if (tempLength) {
				x /= tempLength;
				y /= tempLength;
				z /= tempLength;
			}
		}

	};

	class Triangle3D {
	public:
		Point3D v[3];

		void set(const Point3D &p1, const Point3D &p2, const Point3D &p3) {
			v[0] = p1;
			v[1] = p2;
			v[2] = p3;
		}

		Triangle3D() {
			Point3D p1, p2, p3;
			set(p1, p2, p3);
		}

		Triangle3D(const Point3D &p1, const Point3D &p2, const Point3D &p3) {
			set(p1, p2, p3);
		}
	};

	inline Point& operator+=(Point& a, const Point& b) {
		a.x += b.x;
		a.y += b.y;
		return a;
	}

	inline bool operator==(const Point& a, const Point& b) {
		return (a.x == b.x && a.y == b.y) ? true : false;
	}

	inline bool operator!=(const Point& a, const Point& b) {
		return (a.x == b.x && a.y == b.y) ? false : true;
	}

	inline Point operator+(const Point& a, const Point& b) {
		return Point(a.x + b.x, a.y + b.y);
	}

	inline Point operator-(const Point& a, const Point& b) {
		return Point(a.x - b.x, a.y - b.y);
	}

	inline Point operator*(const VGfloat& a, const Point& b) {
		return Point(a * b.x, a * b.y);
	}

	inline float operator*(const Point3D& a, const Point3D& b) { return a.dot(b); }

	inline float operator*(const Point& a, const Point& b) { return a.dot(b); }

	inline Point3D operator-(const Point3D& a, const Point3D& b) {
		return Point3D(a.x - b.x, a.y - b.y, a.z - b.z);
	}

	// A roundoff factor in the cubic classification and texture coordinate
	// generation algorithms. It primarily determines the handling of corner
	// cases during the classification process. Be careful when adjusting it;
	// it has been determined empirically to work well. When changing it, you
	// should look in particular at shapes that contain quadratic curves and
	// ensure they still look smooth. Once pixel tests are running against this
	// algorithm, they should provide sufficient coverage to ensure that
	// adjusting the constant won't break anything.

	namespace {
		const float Epsilon = 5.0e-4f;
//		const float Epsilon = 8.0e-4f;
	}

	inline float roundToZero(float val) {
		if (val < Epsilon && val > -Epsilon)
			return 0.0f;
		return val;
	}

	inline bool approxEqual(const Point& v0, const Point& v1) {
		return (v0 - v1).lengthSquared() < Epsilon * Epsilon;
	}

	inline bool approxEqual(const Point3D& v0, const Point3D& v1) {
		return (v0 - v1).lengthSquared() < Epsilon * Epsilon;
	}

	inline bool approxEqual(float f0, float f1) {
		return fabsf(f0 - f1) < Epsilon;
	}

	inline int orientation(const Point& p1,
			       const Point& p2,
			       const Point& p3)
	{
		float crossProduct = (p2.y - p1.y) * (p3.x - p2.x) - (p3.y - p2.y) * (p2.x - p1.x);
		return (crossProduct < 0.0f) ? -1 : ((crossProduct > 0.0f) ? 1 : 0);
	}

	inline bool linesIntersect(const Point& p1,
				   const Point& q1,
				   const Point& p2,
				   const Point& q2)
	{
		return (orientation(p1, q1, p2) != orientation(p1, q1, q2)
			&& orientation(p2, q2, p1) != orientation(p2, q2, q1));
	}

	// Returns true if the lines intersect. Intersection point is stored in intersection_at
	inline bool get_line_intersection(const Point &p0, const Point &p1,
					  const Point &p2, const Point &p3,
					  Point &intersection_at) {
		Point s1 = p1 - p0;
		Point s2 = p3 - p2;

		VGfloat denominator = -s2.x * s1.y + s1.x * s2.y;

		// if the denominator is 0 means that the segments are parallell or colinear (we treat both cases as no intersection)
		if(!(denominator == 0)) {
			VGfloat s, t;
			s = (-s1.y * (p0.x - p2.x) + s1.x * (p0.y - p2.y)) / denominator;
			t = ( s2.x * (p0.y - p2.y) - s2.y * (p0.x - p2.x)) / denominator;

			if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
				// Segments intersect
				intersection_at = p0 + t * s1;
				return true;
			}
		}
		return false;
	}
}

#endif
