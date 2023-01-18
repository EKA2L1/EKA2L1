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

#ifndef __GNUVG_PAINT_HH
#define __GNUVG_PAINT_HH

#include <dispatch/libraries/vg/consts.h>

#include <dispatch/libraries/vg/gnuVG_config.hh>
#include <dispatch/libraries/vg/gnuVG_object.hh>

namespace gnuVG {

	class Image;

	class Color {
	public:
		union {
			struct {
				VGfloat r,g,b,a;
			};
			VGfloat c[4];
		};

		Color() : r(0.0f), g(0.0f), b(0.0f), a(0.0f) {}
	};

	class Paint : public Object {
	public:
		Paint(Context *context);

		VGPaintType ptype = VG_PAINT_TYPE_COLOR;

		Color color;

		// Gradient parameters - depending on type
		//   linear: x0, y0, normal.x, normal.y, length
		//   radial: cx, cy, fx - cx, fy - cy, r ^ 2, 1 / (r^2 − (fx'^2 + fy'^2))
		VGfloat gradient_parameters[6];

		// Each stop is 6 values - (offset), (invfactor) and (R, G, B, A)
		VGfloat color_ramp_stop_offset[GNUVG_MAX_COLOR_RAMP_STOPS];
		VGfloat color_ramp_stop_invfactor[GNUVG_MAX_COLOR_RAMP_STOPS];
		VGfloat color_ramp_stop_color[GNUVG_MAX_COLOR_RAMP_STOPS * 4];
		VGColorRampSpreadMode spread_mode = VG_COLOR_RAMP_SPREAD_PAD;
		int max_stops = 0;
		bool premultiplied;

		// pattern image
		VGTilingMode tiling_mode = VG_TILE_FILL;
		std::shared_ptr<Image> pattern;

		/* OpenVG equivalent API - Paint Manipulation */
		void vgSetColor(VGuint rgba);
		VGuint vgGetColor();

		/* inherited virtual interface */
		virtual void vgSetParameterf(VGint paramType, VGfloat value);
		virtual void vgSetParameteri(VGint paramType, VGint value);
		virtual void vgSetParameterfv(VGint paramType, VGint count, const VGfloat *values);
		virtual void vgSetParameteriv(VGint paramType, VGint count, const VGint *values);

		virtual VGfloat vgGetParameterf(VGint paramType);
		virtual VGint vgGetParameteri(VGint paramType);

		virtual VGint vgGetParameterVectorSize(VGint paramType);

		virtual void vgGetParameterfv(VGint paramType, VGint count, VGfloat *values);
		virtual void vgGetParameteriv(VGint paramType, VGint count, VGint *values);

		virtual void vgPaintPattern(std::shared_ptr<Image> pattern);
	};
}

#endif
