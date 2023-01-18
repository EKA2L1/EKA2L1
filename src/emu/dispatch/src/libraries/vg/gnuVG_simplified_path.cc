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

#include <dispatch/libraries/vg/consts.h>

#include <dispatch/libraries/vg/gnuVG_simplified_path.hh>
#include <dispatch/libraries/vg/gnuVG_context.hh>
#include <dispatch/libraries/vg/gnuVG_debug.hh>

#include <vector>

// Max recursion depth for cubic subdivision
#define MAX_RENDER_SUBDIVISION 16

// Define pixel size factor for subdivision limit
#define PIXEL_SIZE_FACTOR 0.125f

namespace gnuVG {
	/*********************************************************
	 *
	 * Stroke shape generation data
	 *
	 *********************************************************/
	enum JoinStyle {
		no_join, join_miter, join_round, join_bevel
	};

	static Point pen, last_direction, first_direction;
	static JoinStyle join_style, default_join_style;

	static Point contour_start;
	static bool start_new_contour;

	static unsigned int nr_vertices;
	static std::vector<VGfloat> v_array; // vertices
	static std::vector<unsigned int> t_array; // triangle vertice indices
	static unsigned int previous_segment[4]; // indices for previous segment
	static unsigned int current_segment[4]; // indices for current segment
	static unsigned int first_segment[4]; // indices for current segment

	static VGfloat stroke_width = 1.0f;
	static VGfloat miter_limit = 1.0f;

	static std::vector<VGfloat> dash_pattern;
	static VGfloat dash_segment_phase_left; // the dash pattern is divided into a set of on and off segments of specific length, this indicates how much is left of the current segment
	static size_t dash_segment_index; // even index == dash ON, uneven index = dash OFF


	/*********************************************************
	 *
	 * "Endpoint -> Center" parameterization helpers
	 * code is from the OpenVG 1.1 specification, Appendix A
	 *
	 *********************************************************/
	/*  Given: Points (x0, y0) and (x1, y1)
	 * Return: TRUE if a solution exists, FALSE otherwise
	 *         Circle centers are written to (cx0, cy0) and (cx1, cy1)
	 */
	static inline VGboolean findUnitCircles(double x0, double y0, double x1, double y1,
						double *cx0, double *cy0,
						double *cx1, double *cy1) {
		/* Compute differences and averages */
		double dx = x0 - x1;
		double dy = y0 - y1;
		double xm = (x0 + x1) / 2.0;
		double ym = (y0 + y1) / 2.0;
		double dsq, disc, s, sdx, sdy;

		/* Solve for intersecting unit circles */
		dsq = dx*dx + dy*dy;
		if (dsq == 0.0)
			return VG_FALSE; /* Points are coincident */
		disc = 1.0 / dsq - 1.0 / 4.0;
		if (disc < 0.0)
			return VG_FALSE; /* Points are too far apart */

		s = sqrt(disc);
		sdx = s*dx;
		sdy = s*dy;
		*cx0 = xm + sdy;
		*cy0 = ym - sdx;
		*cy0 = ym - sdx;
		*cx1 = xm - sdy;
		*cy1 = ym + sdx;
		return VG_TRUE;
	}

	/*  Given: Ellipse parameters rh, rv, rot (in degrees),
	 *         endpoints (x0, y0) and (x1, y1)
	 * Return: TRUE if a solution exists, FALSE otherwise
	 * Ellipse centers are written to (cx0, cy0) and (cx1, cy1)
	 */
	static inline VGboolean findEllipses(double rh, double rv, double rot,
					     const Point &p0, const Point &p1,
					     Point& c0, Point& c1) {
		double COS, SIN, x0p, y0p, x1p, y1p, pcx0, pcy0, pcx1, pcy1;
		/* Convert rotation angle from degrees to radians */
		rot *= M_PI / 180.0;
		/* Pre-compute rotation matrix entries */
		COS = cos(rot); SIN = sin(rot);
		/* Transform (x0, y0) and (x1, y1) into unit space */
		/* using (inverse) rotate, followed by (inverse) scale   */
		x0p = (p0.x*COS + p0.y*SIN) / rh;
		y0p = (-p0.x*SIN + p0.y*COS) / rv;
		x1p = (p1.x*COS + p1.y*SIN) / rh;
		y1p = (-p1.x*SIN + p1.y*COS) / rv;

		if (!findUnitCircles(x0p, y0p, x1p, y1p,
				     &pcx0, &pcy0, &pcx1, &pcy1))
			return VG_FALSE;

		/* Transform back to original coordinate space  */
		/* using (forward) scale followed by (forward) rotate */
		pcx0 *= rh; pcy0 *= rv;
		pcx1 *= rh; pcy1 *= rv;
		c0.x = pcx0*COS - pcy0*SIN;
		c0.y = pcx0*SIN + pcy0*COS;
		c1.x = pcx1*COS - pcy1*SIN;
		c1.y = pcx1*SIN + pcy1*COS;
		return VG_TRUE;
	}

	void SimplifiedPath::approximate_arc(
		std::function<void(VGfloat x, VGfloat y)> bbox_modifier,
		const VGfloat *dat,
		Point &pen,
		bool relative,
		bool counter_clockwise,
		bool large) {
		static VGfloat coords[] = {
			1.0f,     kappa(),
			kappa(),  1.0f,
			0.0f,     1.0f,

			-kappa(), 1.0f,
			-1.0f,    kappa(),
			-1.0f,    0.0f,

			-1.0f,   -kappa(),
			-kappa(), -1.0f,
			0.0f,     -1.0f,

			kappa(),  -1.0f,
			1.0f,    -kappa(),
			1.0f,      0.0f
		};

		auto rh = dat[0];
		auto rv = dat[1];
		auto rot = dat[2];
		Point end_point(dat[3], dat[4]);

		if(relative) {
			end_point += pen;
		}

		if(pen == end_point)
			return;

		Point c0, c1;
		VGboolean success = findEllipses(rh, rv, rot,
						 pen, end_point,
						 c0, c1);

		Matrix rmat;

		GNUVG_DEBUG("type(%s)\n", counter_clockwise ? "counter clockwise" : "clockwise");
		GNUVG_DEBUG("path(%s)\n", large ? "large" : "small");
		GNUVG_DEBUG("rh, rv(%f, %f)\n", rh, rv);
		GNUVG_DEBUG("rot(%f)\n", rot);
		GNUVG_DEBUG("pen(%f, %f)\n", pen.x, pen.y);
		GNUVG_DEBUG("end(%f, %f)\n", end_point.x, end_point.y);
		if(success) {

			Point center;
			bool is_left;
			if(counter_clockwise)
				is_left = large ? false : true;
			else
				is_left = large ? true : false;
			center = is_left ? c0 : c1;

			GNUVG_DEBUG("c0(%f, %f)\n", c0.x, c0.y);
			GNUVG_DEBUG("c1(%f, %f)\n", c1.x, c1.y);
			GNUVG_DEBUG("center(%f, %f)\n", center.x, center.y);

			// get vectors
			Point v1 = pen - center;
			Point v2 = end_point - center;

			v1.normalize();
			v2.normalize();

			// calculate small arc length, in radians
			auto angle = v1.angle_with(v2);
			auto absang = angle < 0.0f ? (-angle) : (angle);

			rmat.translate(center.x, center.y);

			Point h(1.0f, 0.0f);
			auto quad_rotation = h.angle_with(v1);
			GNUVG_DEBUG("v1(%f, %f)\n", v1.x, v1.y);
			GNUVG_DEBUG("dot_product(%f)\n", h.dot(v1) / (h.length() * v1.length()));
			GNUVG_DEBUG("quad_rotation(%f degrees)\n", 180.0f * quad_rotation / M_PI);
			rmat.rotate(quad_rotation);

			if(!counter_clockwise)
				rmat.scale(1.0f, -1.0f); // flip it
			if(large)
				absang = 2.0f * M_PI - absang;

			auto quadrants_f = 2.0f * absang / M_PI;
			auto quadrants = (int)quadrants_f;
			auto start_angle = ((VGfloat)quadrants) * (M_PI / 2.0f);
			auto last_angle = roundToZero(absang - start_angle);

			GNUVG_DEBUG("quadrants: %d\n", quadrants);
			GNUVG_DEBUG("abs angle: %f\n", absang);
			GNUVG_DEBUG("start angle: %f\n", start_angle);
			GNUVG_DEBUG("last angle: %f\n", last_angle);

			rmat.scale(rh, rv);

			Point last_qp(1.0f, 0.0f);
			for(int k = 0; k < quadrants; k++) {
				int i = k * 6;
				Point c1(coords[i    ], coords[i + 1]);
				Point c2(coords[i + 2], coords[i + 3]);
				Point ep(coords[i + 4], coords[i + 5]);

				Point sp_m = rmat.map_point(last_qp);
				Point c1_m = rmat.map_point(c1);
				Point c2_m = rmat.map_point(c2);
				Point ep_m = rmat.map_point(ep);

				GNUVG_DEBUG("add quadrant: p%d = [%f %f; %f %f; %f %f; %f %f]\n",
					    i,
					    last_qp.x, last_qp.y,
					    c1.x, c1.y,
					    c2.x, c2.y,
					    ep.x, ep.y);
				GNUVG_DEBUG("add quadrant mod: p%d = [%f %f; %f %f; %f %f; %f %f]\n",
					    i,
					    sp_m.x, sp_m.y,
					    c1_m.x, c1_m.y,
					    c2_m.x, c2_m.y,
					    ep_m.x, ep_m.y);
				last_qp = ep;
				add_cubic(c1_m, c2_m, ep_m);
				pen = ep_m;
				bbox_modifier(c1_m.x, c1_m.y);
				bbox_modifier(c2_m.x, c2_m.y);
				bbox_modifier(ep_m.x, ep_m.y);
			}

			if(last_angle > 0.0f) {
				Matrix simple;

				simple.rotate(start_angle);

				auto f = (4.0f / 3.0f) * tanf(last_angle / 4.0f);
				Point c1(1.0f, f);
				Point c2(cosf(last_angle) + f * sinf(last_angle),
					 sinf(last_angle) - f * cosf(last_angle));
				Point e(cosf(last_angle), sinf(last_angle));

				auto c1sm = simple.map_point(c1);
				auto c2sm = simple.map_point(c2);
				auto esm = simple.map_point(e);

				Point c1_m = rmat.map_point(c1sm);
				Point c2_m = rmat.map_point(c2sm);

				Point ssm = simple.map_point(Point(1.0f, 0.0f));
				Point sm = rmat.map_point(ssm);
				GNUVG_DEBUG("add partial no rot: pP = [%f %f; %f, %f; %f %f; %f %f]\n",
					    1.0f, 0.0f,
					    c1.x, c1.y,
					    c2.x, c2.y,
					    e.x, e.y);
				GNUVG_DEBUG("add partial: p4 = [%f %f; %f, %f; %f %f; %f %f]\n",
					    ssm.x, ssm.y,
					    c1sm.x, c1sm.y,
					    c2sm.x, c2sm.y,
					    esm.x, esm.y);
				GNUVG_DEBUG("add partial mod: p4 = [%f %f; %f, %f; %f %f; %f %f]\n",
					    sm.x, sm.y,
					    c1_m.x, c1_m.y,
					    c2_m.x, c2_m.y,
					    end_point.x, end_point.y);

				add_cubic(c1_m, c2_m, end_point);
				pen = end_point;
				bbox_modifier(c1_m.x, c1_m.y);
				bbox_modifier(c2_m.x, c2_m.y);
				bbox_modifier(end_point.x, end_point.y);
			}
		} else {
			Point v = end_point - pen;
			Point h(-1.0f, 0.0f);
			auto rotation = h.angle_with(v);
			Point c = pen + 0.5f * v;

			rmat.translate(c.x, c.y);

			// assume points are far away => draw half a circle
			rmat.rotate(rotation);

			rmat.scale(rh, rv);
			if(!counter_clockwise)
				rmat.scale(1.0f, -1.0f);

			for(int k = 0; k < 2; k++) {
				int i = k * 6;
				Point c1(coords[i    ], coords[i + 1]);
				Point c2(coords[i + 2], coords[i + 3]);
				Point ep(coords[i + 4], coords[i + 5]);

				Point c1_m = rmat.map_point(c1);
				Point c2_m = rmat.map_point(c2);
				Point ep_m = rmat.map_point(ep);

				GNUVG_DEBUG("add quadrant: (%f, %f), (%f, %f), (%f, %f)\n",
					    c1_m.x, c1_m.y,
					    c2_m.x, c2_m.y,
					    ep_m.x, ep_m.y);
				add_cubic(c1_m, c2_m, end_point);
				pen = end_point;
				bbox_modifier(c1_m.x, c1_m.y);
				bbox_modifier(c2_m.x, c2_m.y);
				bbox_modifier(end_point.x, end_point.y);
			}
		}
	}

	void SimplifiedPath::simplify_path(const VGubyte* pathSegments,
					   const VGfloat* pathData,
					   VGint numSegments) {
		GNUVG_DEBUG("SimplifiedPath::simplify_path()\n");
		VGint remaining_segments = numSegments;
		const VGubyte *sgmt = pathSegments;
		const VGfloat *dat = pathData;

		std::function<void(VGfloat x, VGfloat y)> bbox_modifier;
		auto bbox_modifier_start = [this](VGfloat x, VGfloat y) {
			bounding_box[0].x = x;
			bounding_box[0].y = y;
			bounding_box[1].x = x;
			bounding_box[1].y = y;
		};
		auto bbox_modifier_rest = [this](VGfloat x, VGfloat y) {
			if(bounding_box[0].x > x)
				bounding_box[0].x = x;
			if(bounding_box[0].y > y)
				bounding_box[0].y = y;
			if(bounding_box[1].x < x)
				bounding_box[1].x = x;
			if(bounding_box[1].y < y)
				bounding_box[1].y = y;
		};

		Point ctr_start(0.0f, 0.0f); // start of current contour in the path
		Point pen; // current position of the pen

		segments.clear();

		while(remaining_segments) {
			GNUVG_DEBUG("SimplifiedPath::simplify_path() left: %d\n", remaining_segments);
			if(segments.size() == 0)
				bbox_modifier = bbox_modifier_start;
			else
				bbox_modifier = bbox_modifier_rest;

			auto relative = ((*sgmt) & 0x00000001) == 0x00000001 ? true : false;
			auto segtype = (*sgmt) & (~0x00000001);

			if(segtype == VG_CLOSE_PATH) {
				GNUVG_DEBUG("SimplifiedPath::simplify_path() close path\n");
				Segment ns;
				ns.t = sp_close;
				segments.push_back(ns);
				pen = ctr_start;
			} else if(segtype == VG_MOVE_TO) {
				GNUVG_DEBUG("SimplifiedPath::simplify_path() move to\n");
				pen = ctr_start = Point(dat[0], dat[1]);

				Segment ns;
				ns.t = sp_move_to;
				ns.ep = pen;

				segments.push_back(ns);

				bbox_modifier(dat[0], dat[1]);
				dat = &dat[2];
			} else {
				switch(segtype) {
					/* cases already handled */
				case VG_CLOSE_PATH:
				case VG_MOVE_TO:
					break;

					/* commands with ONE parameter */
				case VG_HLINE_TO:
				{
					GNUVG_DEBUG("HLineTo segment found\n");
					Point end_point(dat[0], pen.y);
					if(relative) {
						end_point.x += pen.x;
					}

					Segment ns;
					ns.t = sp_line_to;
					ns.ep = end_point;

					segments.push_back(ns);

					pen = end_point;
					bbox_modifier(end_point.x, end_point.y);
					dat = &dat[1];
				}
				break;
				case VG_VLINE_TO:
				{
					GNUVG_DEBUG("VLineTo segment found.\n");
					Point end_point(pen.x, dat[0]);
					if(relative) {
						end_point.y += pen.y;
					}

					Segment ns;
					ns.t = sp_line_to;
					ns.ep = end_point;

					segments.push_back(ns);
					pen = end_point;
					bbox_modifier(end_point.x, end_point.y);
					dat = &dat[1];
				}
				break;

				/* commands with TWO parameters */
				case VG_SQUAD_TO:
				{
					GNUVG_ERROR("SQuadTo segment found - not supported.\n");
					dat = &dat[2];
				}
				break;
				case VG_LINE_TO:
				{
					Point end_point(dat[0], dat[1]);
					if(relative) {
						end_point += pen;
					}

					Segment ns;
					ns.t = sp_line_to;
					ns.ep = end_point;

					segments.push_back(ns);
					pen = end_point;
					bbox_modifier(end_point.x, end_point.y);
					GNUVG_DEBUG("LineTo segment found.\n");

					dat = &dat[2];
				}
				break;

				/* commands with FOUR parameters */
				case VG_SCUBIC_TO:
					GNUVG_ERROR("SCubicTo segment found - not supported.\n");
					dat = &dat[4];
					break;
				case VG_QUAD_TO:
				{
					GNUVG_DEBUG("QuadTo segment found, convert to cubic\n");

					Point q0 = pen;
					Point q1(dat[0], dat[1]);
					Point q2(dat[2], dat[3]);

					if(relative) {
						q1 += pen;
						q2 += pen;
					}

					Point c1 = q0 + (2.0f/3.0f) * (q1 - q0);
					Point c2 = q2 + (2.0f/3.0f) * (q1 - q2);
					Point end_point = q2;

					GNUVG_DEBUG("   ([%f, %f][%f, %f][%f, %f]) -> ([%f, %f][%f, %f][%f, %f][%f, %f])\n",
						    q0.x, q0.y, q1.x, q1.y, q2.x, q2.y,
						    pen.x, pen.y, c1.x, c1.y, c2.x, c2.y,
						    end_point.x, end_point.y);

					add_cubic(c1, c2, end_point);

					pen = end_point;
					bbox_modifier(c1.x, c1.y);
					bbox_modifier(c2.x, c2.y);
					bbox_modifier(end_point.x, end_point.y);
					dat = &dat[4];
				}
				break;

				/* commands with FIVE parameters */
				case VG_SCCWARC_TO:
					GNUVG_DEBUG("SimplifiedPath::simplify_path() sccwarc\n");
					approximate_arc(bbox_modifier,
							dat,
							pen, relative,
							true, false);
					dat = &dat[5];
					break;
				case VG_SCWARC_TO:
					GNUVG_DEBUG("SimplifiedPath::simplify_path() scwarc\n");
					approximate_arc(bbox_modifier,
							dat,
							pen, relative,
							false, false);
					dat = &dat[5];
					break;
				case VG_LCCWARC_TO:
					GNUVG_DEBUG("SimplifiedPath::simplify_path() lccwarc\n");
					approximate_arc(bbox_modifier,
							dat,
							pen, relative,
							true, true);
					dat = &dat[5];
					break;
				case VG_LCWARC_TO:
					GNUVG_DEBUG("SimplifiedPath::simplify_path() lcwarc\n");
					approximate_arc(bbox_modifier,
							dat,
							pen, relative,
							false, true);
					dat = &dat[5];
					break;

					/* commands with SIX parameters */
				case VG_CUBIC_TO:
				{
					GNUVG_DEBUG("CubicTo segment found.\n");
					Point c1(dat[0], dat[1]);
					Point c2(dat[2], dat[3]);
					Point end_point(dat[4], dat[5]);
					if(relative) {
						c1 += pen;
						c2 += pen;
						end_point += pen;
					}

					add_cubic(c1, c2, end_point);
					pen = end_point;
					bbox_modifier(c1.x, c1.y);
					bbox_modifier(c2.x, c2.y);
					bbox_modifier(end_point.x, end_point.y);
					dat = &dat[6];
				}
				break;
				}
			}
			sgmt++;
			remaining_segments--;
		}
	}

	void SimplifiedPath::process_path(
		std::function<void(Point)> add_vertice,
		std::function<void(bool)> finish_contour,
		std::function<void(
			const Point& s,
			const Point& c1,
			const Point& c2,
			const Point& e)> process_curve) {
		auto s = segments.data();
		auto max_k = segments.size();
		auto k = max_k;
		auto unfinished_contour = false;
		Point pen(0.0f, 0.0f);
		for(k = 0; k < max_k; k++) {
			switch(s[k].t) {
			case sp_close:
				if(unfinished_contour)
					finish_contour(true);
				unfinished_contour = false;
				break;

			case sp_move_to:
				if(unfinished_contour)
					finish_contour(false);
				unfinished_contour = false;
				add_vertice(s[k].ep);
				break;

			case sp_line_to:
				unfinished_contour = true;
				add_vertice(s[k].ep);
				break;

			case sp_cubic_to:
				unfinished_contour = true;
				process_curve(pen, s[k].c1,
					      s[k].c2, s[k].ep);
				break;
			}
			pen = s[k].ep;
		}
		if(unfinished_contour)
			finish_contour(false);
	}

	static Point calculate_pixelsize(Context *context) {
		Point pixel_size;
		auto x_mapped = context->map_point(Point(1.0f, 0.0f));
		auto y_mapped = context->map_point(Point(0.0f, 1.0f));
		auto origo_mapped = context->map_point(Point(0.0f, 0.0f));

		pixel_size.x = PIXEL_SIZE_FACTOR / (x_mapped - origo_mapped).length();
		pixel_size.y = PIXEL_SIZE_FACTOR / (y_mapped - origo_mapped).length();

		return pixel_size;
	}

	static void flatten_curve(const Point& pixel_size,
				  const Point& s,
				  const Point& c1,
				  const Point& c2,
				  const Point& e,
				  std::function<void(const Point& p)> point_created) {
		Point d1, d2;

		struct CBez {
			Point points[4];
		};

		CBez cubics[MAX_RENDER_SUBDIVISION];
		CBez *c, *cleft, *cright;
		int cindex = 0;

		cubics[0].points[0] = s;
		cubics[0].points[1] = c1;
		cubics[0].points[2] = c2;
		cubics[0].points[3] = e;

		// we need -1 for comparison
		constexpr auto recursion_depth = MAX_RENDER_SUBDIVISION - 1;

		while (cindex >= 0) {
			c = &cubics[cindex];
			const Point *p = c->points;

			/* Calculate distance of control points from their
			   counterparts on the line between end points */
			d1 = 3.0f * p[1] - 2.0f * p[0] - p[3]; d1.x *= d1.x; d1.y *= d1.y;
			d2 = 3.0f * p[2] - 2.0f * p[3] - p[0]; d2.x *= d2.x; d2.y *= d2.y;

			if (d1.x < d2.x) d1.x = d2.x;
			if (d1.y < d2.y) d1.y = d2.y;

			/* Cancel if the curve is flat enough */
			if (((d1.x <= pixel_size.x) && (d1.y <= pixel_size.y))
			    || cindex == recursion_depth) {
				point_created(p[3]);
				--cindex;
			} else {
				/* Left recursion goes on top of stack! */
				cright = c; cleft = &cubics[++cindex];

				Point mm, c1, c2, c3, c4, c5;

				/* Subdivide into 2 sub-curves (variant on Casteljau's Algorithm) */
				c1 = 0.5f * (p[0] + p[1]);
				mm = 0.5f * (p[1] + p[2]);
				c5 = 0.5f * (p[2] + p[3]);

				c2 = 0.5f * (c1 + mm);
				c4 = 0.5f * (mm + c5);

				c3 = 0.5f * (c2 + c4);

				/* Add left recursion to stack */
				cleft->points[0] = p[0];
				cleft->points[1] = c1;
				cleft->points[2] = c2;
				cleft->points[3] = c3;

				/* Add right recursion to stack */
				cright->points[0] = c3;
				cright->points[1] = c4;
				cright->points[2] = c5;
				cright->points[3] = p[3];
			}
		}
	}

	void SimplifiedPath::tesselate_fill_shape(TESStesselator* tess) {
		static std::vector<VGfloat> outline_vertices_vector(4);

		VGfloat *outline_vertices = outline_vertices_vector.data();
		int outline_floats_count = 0, max_floats_capacity = outline_vertices_vector.size();

		auto add_vertice =
			[this, &outline_vertices,
			 &outline_floats_count, &max_floats_capacity](const Point &p) {
			if(outline_floats_count == max_floats_capacity) {
				outline_vertices_vector.resize(outline_vertices_vector.size() * 2);
				outline_vertices = outline_vertices_vector.data();
				max_floats_capacity = outline_vertices_vector.size();
			}
			outline_vertices[outline_floats_count++] = p.x;
			outline_vertices[outline_floats_count++] = p.y;
		};

		auto finalize_contour =
			[this, &outline_vertices,
			 tess,
			 &outline_floats_count, &max_floats_capacity](bool /*do_close*/) {
			if(outline_floats_count) {
				tessAddContour(tess, 2,
					       outline_vertices,
					       sizeof(VGfloat)*2,
					       outline_floats_count >> 1);
				outline_floats_count = 0;
			}
		};

		auto pixsize = calculate_pixelsize(context);
		auto process_curve =
			[pixsize, add_vertice](
				const Point& s,
				const Point& c1,
				const Point& c2,
				const Point& e) {
			flatten_curve(pixsize, s, c1, c2, e, add_vertice);
		};

		process_path(
			add_vertice, finalize_contour, process_curve
			);
	}

	static void push_vertice(const Point &p) {
		v_array.push_back(p.x);
		v_array.push_back(p.y);
		++nr_vertices;
	}

	static void push_triangle(const unsigned int vertice_index[]) {
		for(int k = 0; k < 3; k++)
			t_array.push_back(vertice_index[k]);
	}

	static void add_miter_join(VGfloat angle, const Point &last_normal, const Point &new_normal) {
		// convert the angle between the direction into the angle
		// between the old line segment and the new
		VGfloat line_angle = angle < 0.0 ? M_PI + angle : M_PI - angle; // make sure it's a positive

		// calculate the miter size
		VGfloat miter_factor = 1.0f / sin(line_angle / 2.0f);
		if(miter_factor > miter_limit) return; // beyond the limit, don't do miter join
		VGfloat miter_size = miter_factor * stroke_width;

		// calculate the additional triangle that the miter adds
		miter_size /= 2.0f;
		line_angle /= 2.0f;
		miter_size *= cos(line_angle);

		// check which side the miter should be on
		unsigned int old_point_index;
		if(angle < 0.0f) {
			unsigned int triangle[] = {
				previous_segment[1], nr_vertices, current_segment[0]
			};
			push_triangle(triangle);
			old_point_index = triangle[0] << 1;
		} else {
			unsigned int triangle[] = {
				previous_segment[3], current_segment[2], nr_vertices
			};
			push_triangle(triangle);
			old_point_index = triangle[0] << 1;
		}
		Point old_point(v_array[old_point_index], v_array[old_point_index + 1]);
		miter_size /= last_direction.length();
		Point triangle_tip = old_point + miter_size * last_direction;
		push_vertice(triangle_tip);
	}

	static void add_bevel_join(VGfloat angle, const Point &last_normal, const Point &new_normal) {
		if(angle < 0.0f) { // bevel on left side of direction
			unsigned int triangle[] = {
				previous_segment[1], current_segment[0], previous_segment[3]
			};
			push_triangle(triangle);
		} else { // bevel on right side
			unsigned int triangle[] = {
				previous_segment[3], current_segment[0], current_segment[2]
			};
			push_triangle(triangle);
		}
	}

	static void add_join(const Point &new_direction) {
		if(join_style == no_join) {
			// add no join, but remember this specific direction
			first_direction = new_direction;
			for(unsigned int k = 0; k < 4; k++) {
				first_segment[k] = current_segment[k];
			}
		} else {
			auto width_factor = (stroke_width / last_direction.length()) * 0.5f;
			Point last_normal = width_factor * last_direction.normal();
			Point new_normal = width_factor * new_direction.normal();

			auto cross = last_direction.cross(new_direction);
			auto dot = last_direction.dot(new_direction);
			auto angle = atan2(cross, dot);

			// we only need to create a join if the we have a non-zero angle
			if(angle != 0.0f) {
				switch(join_style) {
				default:
				case no_join: /* this case is already handled above */
					break;

				case join_round:
					// xxx not properly implemented yet
					add_bevel_join(angle, last_normal, new_normal);
					break;
				case join_miter:
					add_bevel_join(angle, last_normal, new_normal);
					add_miter_join(angle, last_normal, new_normal);
					break;
				case join_bevel:
					add_bevel_join(angle, last_normal, new_normal);
					break;
				}
			}
		}
		join_style = join_bevel; // set join_style to bevel until a new segment is completed
		last_direction = new_direction;
	}

	static void close_to_first_segment() {
		for(unsigned int k = 0; k < 4; k++) {
			previous_segment[k] = current_segment[k];
			current_segment[k] = first_segment[k];
		}
	}

	static void push_segment_outline_triangles(const Point &normal, const Point &stroke) {
		for(unsigned int k = 0; k < 4; k++) {
			previous_segment[k] = current_segment[k];
			current_segment[k] = nr_vertices + k;
		}

		unsigned int triangle_a[] = {nr_vertices + 0, nr_vertices + 1, nr_vertices + 2};
		unsigned int triangle_b[] = {nr_vertices + 1, nr_vertices + 3, nr_vertices + 2};

		push_vertice(pen + normal);
		push_vertice(pen + normal + stroke);
		push_vertice(pen - normal);
		push_vertice(pen - normal + stroke);

		push_triangle(triangle_a);
		push_triangle(triangle_b);
	}

	static void create_segment_outline(const Point &p) {
		Point direction = p - pen;
		if(direction.length() == 0) return;

		Point normal = (stroke_width / direction.length()) * 0.5 * Point(-direction.y, direction.x);

		if(dash_pattern.size() == 0) {
			// non-dashed
			push_segment_outline_triangles(normal, direction);
			add_join(direction);
			pen = pen + direction;
		} else {
			// dashed
			VGfloat remaining = direction.length();
			direction = (1 / direction.length()) * direction; // make unit vector

			while(remaining > 0.0f) {
				VGfloat step_length = remaining < dash_segment_phase_left ? remaining : dash_segment_phase_left;
				Point stroke = step_length * direction;

				remaining -= step_length;
				dash_segment_phase_left -= step_length;

				if(dash_segment_index % 2) {
					// dash is OFF
					join_style = no_join;
				} else {
					push_segment_outline_triangles(normal, stroke);
					add_join(direction);
				}
				while(dash_segment_phase_left <= 0.0f) {
					dash_segment_index = (dash_segment_index + 1) % dash_pattern.size();
					dash_segment_phase_left += dash_pattern[dash_segment_index];
				}
				pen = pen + stroke;
			}
		}
	}

	void SimplifiedPath::get_stroke_shape(
		std::function<void(const StrokeData &)> render_callback) {
		start_new_contour = true;

		// store the stroke widht internally
		stroke_width = context->get_stroke_width();

		// store the miter limit internally
		miter_limit = context->get_miter_limit();

		// get default join style
		switch(context->get_join_style()) {
		case VG_JOIN_STYLE_FORCE_SIZE:
			break;
		case VG_JOIN_MITER:
			default_join_style = join_miter;
			break;
		case VG_JOIN_ROUND:
			default_join_style = join_round;
			break;
		case VG_JOIN_BEVEL:
			default_join_style = join_bevel;
			break;
		}

		// get dash pattern
		dash_pattern = context->get_dash_pattern();
		dash_segment_index = 0;

		// prepare dash
		if(dash_pattern.size() > 0) {
			VGfloat dash_phase = context->get_dash_phase();
			dash_segment_index = 0;
			while(dash_phase > dash_pattern[dash_segment_index]) {
				dash_phase -= dash_pattern[dash_segment_index];
				if(dash_segment_index >= dash_pattern.size()) // roll over
					dash_segment_index = 0;
			}
			// calculate what's left of the current dash segment
			dash_segment_phase_left = dash_pattern[dash_segment_index] - dash_phase;
		}

		// first segment should not create a joint
		join_style = no_join;
		v_array.clear();
		t_array.clear();

		auto add_vertice = [this](const Point &p) {
			if(start_new_contour) {
				start_new_contour = false;
				contour_start = pen = p;
				v_array.clear();
				t_array.clear();
				nr_vertices = 0;
			} else {
				create_segment_outline(p);
				join_style = default_join_style;
			}
		};

		auto finalize_contour =
			[this, render_callback](bool do_close) {
			start_new_contour = true;

			if(do_close) {
				create_segment_outline(contour_start);
				join_style = default_join_style;
				close_to_first_segment();
				add_join(first_direction);
			}

			join_style = no_join;

			StrokeData sdat;
			sdat.vertices = v_array.data();
			sdat.indices = t_array.data();
			sdat.nr_vertices = nr_vertices;
			sdat.nr_indices = t_array.size();

			if(sdat.nr_indices > 0)
				render_callback(sdat);
		};

		auto pixsize = calculate_pixelsize(context);
		auto process_curve =
			[pixsize, add_vertice](
				const Point& s,
				const Point& c1,
				const Point& c2,
				const Point& e) {
			flatten_curve(pixsize, s, c1, c2, e, add_vertice);
		};

		process_path(
			add_vertice,
			finalize_contour,
			process_curve
			);
	}
};
