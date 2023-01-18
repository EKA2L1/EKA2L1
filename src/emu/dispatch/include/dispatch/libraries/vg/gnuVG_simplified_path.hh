/*
 * gnuVG - a free Vector Graphics library
 * Copyright (C) 2016 by Anton Persson
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

#pragma once

#include <functional>
#include <tesselator.h>

#include <dispatch/libraries/vg/gnuVG_math.hh>
#include <dispatch/libraries/vg/gnuVG_memclasses.hh>

namespace gnuVG {
	class Context;

	class SimplifiedPath {
	public:
		enum SegmentType {
			sp_close,
			sp_move_to,
			sp_line_to,
			sp_cubic_to
		};

		struct Segment {
			SegmentType t;
			Point c1, c2, ep; // coordinate data - depending on type
		};

		struct StrokeData {
			const VGfloat *vertices;
			const unsigned int *indices;
			uintptr_t nr_vertices, nr_indices;
		};

		std::vector<Segment> segments;

		void simplify_path(const VGubyte* pathSegments,
				   const VGfloat* pathData,
				   VGint numSegments);

		void get_bounding_box(Point* bbox) {
			bbox[0] = bounding_box[0];
			bbox[1] = bounding_box[1];
		}

		void tesselate_fill_shape(TESStesselator* tess);
		void tesselate_fill_loop_n_blinn(VGFillRule rule, TESStesselator* tess);
		void get_stroke_shape(std::function<void(const StrokeData &)> render_callback);

		SimplifiedPath(Context *context)
			: context(context) {
		}

	private:
		// Bounding box data - top left, bottom right
		// we can use these to calculate an on-screen bounding box easier.
		Point bounding_box[2];
		Context *context;

		inline void add_cubic(Point &c1, Point &c2, Point &end_point) {
			Segment ns;
			ns.t = sp_cubic_to;
			ns.c1 = c1;
			ns.c2 = c2;
			ns.ep = end_point;

			segments.push_back(ns);
		}

		void approximate_arc(
			std::function<void(VGfloat x, VGfloat y)> bbox_modifier,
			const VGfloat *dat,
			Point &pen,
			bool relative,
			bool counter_clockwise,
			bool large);

		void process_path(std::function<void(Point)> add_vertice,
				  std::function<void(bool)> finish_contour,
				  std::function<void(
					  const Point& s,
					  const Point& c1,
					  const Point& c2,
					  const Point& e)> process_curve);
	};
};
