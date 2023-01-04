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
 * of the original contents.
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

#include <dispatch/libraries/vg/gnuVG_math.hh>

static inline bool edgeEdgeTest(const gnuVG::Point& v0Delta,
				const gnuVG::Point& v0,
				const gnuVG::Point& u0,
				const gnuVG::Point& u1) {
	// This edge to edge test is based on Franlin Antonio's gem: "Faster
	// Line Segment Intersection", in Graphics Gems III, pp. 199-202.
	float ax = v0Delta.x;
	float ay = v0Delta.y;
	float bx = u0.x - u1.x;
	float by = u0.y - u1.y;
	float cx = v0.x - u0.x;
	float cy = v0.y - u0.y;
	float f = ay * bx - ax * by;
	float d = by * cx - bx * cy;
	if ((f > 0 && d >= 0 && d <= f) || (f < 0 && d <= 0 && d >= f)) {
		float e = ax * cy - ay * cx;

		// This additional test avoids reporting coincident edges, which
		// is the behavior we want.
		if (gnuVG::approxEqual(e, 0) || gnuVG::approxEqual(f, 0) || gnuVG::approxEqual(e, f))
			return false;

		if (f > 0)
			return e >= 0 && e <= f;

		return e <= 0 && e >= f;
	}
	return false;
}

static inline bool edgeAgainstTriangleEdges(const gnuVG::Point& v0,
					    const gnuVG::Point& v1,
					    const gnuVG::Point& u0,
					    const gnuVG::Point& u1,
					    const gnuVG::Point& u2)
{
	gnuVG::Point delta = v1 - v0;
	// Test edge u0, u1 against v0, v1.
	if (edgeEdgeTest(delta, v0, u0, u1))
		return true;
	// Test edge u1, u2 against v0, v1.
	if (edgeEdgeTest(delta, v0, u1, u2))
		return true;
	// Test edge u2, u1 against v0, v1.
	if (edgeEdgeTest(delta, v0, u2, u0))
		return true;
	return false;
}


namespace gnuVG {

	Matrix::Matrix() {
		init_identity();
	}

	void Matrix::init_identity() {
		b = c = d = f = g = h = 0.0f;
		a = e = i = 1.0f;
	}

	void Matrix::translate(VGfloat x, VGfloat y) {
		g = a * x + d * y + g;
		h = b * x + e * y + h;
	}

	void Matrix::scale(VGfloat x, VGfloat y) {
		a = a * x;
		b = b * x;
		d = d * y;
		e = e * y;
	}

	void Matrix::shear(VGfloat shx, VGfloat shy) {
		VGfloat A, B, D, E;
		VGfloat a2, b2, d2, e2;

		a2 = 1.0f; d2 = shx;
		b2 = shy; e2 = 1.0f;

		A = a * a2 + d * b2;
		D = a * d2 + d * e2;
		B = b * a2 + e * b2;
		E = b * d2 + e * e2;

		a = A; d = D;
		b = B; e = E;
	}

	void Matrix::rotate(VGfloat angle_in_radians) {
		VGfloat A, B, D, E;
		VGfloat a2, b2, d2, e2;

		a2 = cos(angle_in_radians); d2 = -sin(angle_in_radians);
		b2 = sin(angle_in_radians); e2 = cos(angle_in_radians);

		A = a * a2 + d * b2;
		D = a * d2 + d * e2;
		B = b * a2 + e * b2;
		E = b * d2 + e * e2;

		a = A; d = D;
		b = B; e = E;
	}

	void Matrix::multiply(const Matrix &A, const Matrix &B) {
		a = A.a * B.a + A.d * B.b + A.g * B.c;
		b = A.b * B.a + A.e * B.b + A.h * B.c;
		c = A.c * B.a + A.f * B.b + A.i * B.c;

		d = A.a * B.d + A.d * B.e + A.g * B.f;
		e = A.b * B.d + A.e * B.e + A.h * B.f;
		f = A.c * B.d + A.f * B.e + A.i * B.f;

		g = A.a * B.g + A.d * B.h + A.g * B.i;
		h = A.b * B.g + A.e * B.h + A.h * B.i;
		i = A.c * B.g + A.f * B.h + A.i * B.i;
	}

	Point Matrix::map_point(const Point &p) {
		return Point(p.x * a + p.y * d + g,
			     p.x * b + p.y * e + h);
	}

	bool Triangle::contains_point(const Point& point) {
		const Point &a = v[0];
		const Point &b = v[1];
		const Point &c = v[2];

		// Algorithm from http://www.blackpawn.com/texts/pointinpoly/default.html
		VGfloat x0 = c.x - a.x;
		VGfloat y0 = c.y - a.y;
		VGfloat x1 = b.x - a.x;
		VGfloat y1 = b.y - a.y;
		VGfloat x2 = point.x - a.x;
		VGfloat y2 = point.y - a.y;

		VGfloat dot00 = x0 * x0 + y0 * y0;
		VGfloat dot01 = x0 * x1 + y0 * y1;
		VGfloat dot02 = x0 * x2 + y0 * y2;
		VGfloat dot11 = x1 * x1 + y1 * y1;
		VGfloat dot12 = x1 * x2 + y1 * y2;
		VGfloat denominator = dot00 * dot11 - dot01 * dot01;
		if (!denominator)
			// Triangle is zero-area. Treat query point as not being inside.
			return false;
		// Compute
		VGfloat inverseDenominator = 1.0f / denominator;
		VGfloat u = (dot11 * dot02 - dot01 * dot12) * inverseDenominator;
		VGfloat v = (dot00 * dot12 - dot01 * dot02) * inverseDenominator;

		return (u > 0.0f) && (v > 0.0f) && (u + v < 1.0f);
	}

	static bool point_in_triangle(const Point &p, const Point &c1, const Point &c2, const Point &c3) {
		Triangle t(c1, c2, c3);
		return t.contains_point(p);
	}

	bool Triangle::overlap_triangle(const Triangle &other) {
		const Point& a1 = v[0];
		const Point& b1 = v[1];
		const Point& c1 = v[2];
		const Point& a2 = other.v[0];
		const Point& b2 = other.v[1];
		const Point& c2 = other.v[2];

		// Derived from coplanar_tri_tri() at
		// http://jgt.akpeters.com/papers/ShenHengTang03/tri_tri.html ,
		// simplified for the 2D case and modified so that overlapping edges
		// do not report overlapping triangles.

		// Test all edges of triangle 1 against the edges of triangle 2.
		if (edgeAgainstTriangleEdges(a1, b1, a2, b2, c2)
		    || edgeAgainstTriangleEdges(b1, c1, a2, b2, c2)
		    || edgeAgainstTriangleEdges(c1, a1, a2, b2, c2))
			return true;
		// Finally, test if tri1 is totally contained in tri2 or vice versa.
		// The paper above only performs the first two point-in-triangle tests.
		// Because we define that triangles sharing a vertex or edge don't
		// overlap, we must perform additional tests to see whether one
		// triangle is contained in the other.
		if (point_in_triangle(a1, a2, b2, c2)
		    || point_in_triangle(a2, a1, b1, c1)
		    || point_in_triangle(b1, a2, b2, c2)
		    || point_in_triangle(b2, a1, b1, c1)
		    || point_in_triangle(c1, a2, b2, c2)
		    || point_in_triangle(c2, a1, b1, c1))
			return true;
		return false;
	}

}
