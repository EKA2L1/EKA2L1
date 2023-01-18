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

#include <dispatch/libraries/vg/gnuVG_paint.hh>
#include <dispatch/libraries/vg/gnuVG_context.hh>
#include <dispatch/libraries/vg/gnuVG_image.hh>
#include <dispatch/libraries/vg/gnuVG_emuutils.hh>

#define CLAMP(x) ((x) < 0.0f ? 0.0f : ((x) > 1.0f ? 1.0f : (x)))

namespace gnuVG {

	Paint::Paint(Context *context)
		: Object(context) {
		
	}

	void Paint::vgSetColor(VGuint rgba) {
		color.r = ((rgba >> 24) & 0xff)/255.0f;
		color.g = ((rgba >> 16) & 0xff)/255.0f;
		color.b = ((rgba >>  8) & 0xff)/255.0f;
		color.a = ( rgba        & 0xff)/255.0f;
	}

	VGuint Paint::vgGetColor() {
		int red, green, blue, alpha;
		VGuint rgba;
		/*
		 * Clamp color and alpha values from vgGetParameterfv to the
		 * [0, 1] range, scale to 8 bits, and round to integer.
		 */
		red = (int)(CLAMP(color.r)*255.0f + 0.5f);
		green = (int)(CLAMP(color.g)*255.0f + 0.5f);
		blue = (int)(CLAMP(color.b)*255.0f + 0.5f);
		alpha = (int)(CLAMP(color.a)*255.0f + 0.5f);
		rgba = (red << 24) | (green << 16) | (blue << 8) | alpha;

		return rgba;
	}

	void Paint::vgSetParameterf(VGint paramType, VGfloat value) {
		switch(paramType) {
		case VG_PAINT_TYPE:
		case VG_PAINT_COLOR_RAMP_SPREAD_MODE:
		case VG_PAINT_COLOR_RAMP_PREMULTIPLIED:
		case VG_PAINT_PATTERN_TILING_MODE:
		case VG_PAINT_COLOR:
		case VG_PAINT_COLOR_RAMP_STOPS:
		case VG_PAINT_LINEAR_GRADIENT:
		case VG_PAINT_RADIAL_GRADIENT:
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			break;
		}
	}

	void Paint::vgSetParameteri(VGint paramType, VGint value) {
		switch(paramType) {
		case VG_PAINT_TYPE:
			switch(value) {
			case VG_PAINT_TYPE_COLOR:
			case VG_PAINT_TYPE_LINEAR_GRADIENT:
			case VG_PAINT_TYPE_RADIAL_GRADIENT:
			case VG_PAINT_TYPE_PATTERN:
				ptype = (VGPaintType)value;
				return;
			}
			break;

		case VG_PAINT_COLOR_RAMP_SPREAD_MODE:
			switch(value) {
			case VG_COLOR_RAMP_SPREAD_PAD:
			case VG_COLOR_RAMP_SPREAD_REPEAT:
			case VG_COLOR_RAMP_SPREAD_REFLECT:
				spread_mode = (VGColorRampSpreadMode)value;
				return;
			}
			break;

		case VG_PAINT_COLOR_RAMP_PREMULTIPLIED:
			premultiplied = (VGboolean)value == VG_TRUE ? true : false;
			return;

		case VG_PAINT_PATTERN_TILING_MODE:
			switch(value) {
			case VG_TILE_FILL:
			case VG_TILE_PAD:
			case VG_TILE_REPEAT:
			case VG_TILE_REFLECT:
				tiling_mode = (VGTilingMode)value;
				return;
			}
			break;

		case VG_PAINT_COLOR:
		case VG_PAINT_COLOR_RAMP_STOPS:
		case VG_PAINT_LINEAR_GRADIENT:
		case VG_PAINT_RADIAL_GRADIENT:
			break;
		}
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Paint::vgSetParameterfv(VGint paramType, VGint count, const VGfloat *values) {
		switch(paramType) {
		case VG_PAINT_TYPE:
		case VG_PAINT_COLOR_RAMP_SPREAD_MODE:
		case VG_PAINT_COLOR_RAMP_PREMULTIPLIED:
		case VG_PAINT_PATTERN_TILING_MODE:
			break;
		case VG_PAINT_COLOR:
			if(count == 4) {
				for(int k = 0; k < 4; k++) {
					color.c[k] = values[k];
				}
			} else {
				context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			}
			return;

		case VG_PAINT_COLOR_RAMP_STOPS:
			if(count > 0) {
				auto new_stops = count / 5;
				max_stops = new_stops > GNUVG_MAX_COLOR_RAMP_STOPS ?
					GNUVG_MAX_COLOR_RAMP_STOPS : new_stops;

				int k = 0; int c = 0; int v_off = 0;
				for(;
				    k < max_stops;
				    k++, c += 4, v_off += 5) {
					// get offset
					color_ramp_stop_offset[k] = values[v_off];

					// calculate inversion factor
					if(k + 1 < max_stops)
						color_ramp_stop_invfactor[k] =
							1.0f / (values[v_off + 5] - values[v_off]);
					else
						color_ramp_stop_invfactor[k] = 1.0f;

					// get rgba
					color_ramp_stop_color[c + 0] = values[v_off + 1];
					color_ramp_stop_color[c + 1] = values[v_off + 2];
					color_ramp_stop_color[c + 2] = values[v_off + 3];
					color_ramp_stop_color[c + 3] = values[v_off + 4];
				}
			} else {
				max_stops = 0;
			}
			return;

		case VG_PAINT_LINEAR_GRADIENT:
			if(count == 4) {
				/* start position is just copied */
				gradient_parameters[0] = values[0];
				gradient_parameters[1] = values[1];

				/* calculate the direction vector */
				auto dirv_x = values[2] - values[0];
				auto dirv_y = values[3] - values[1];

				/* calculate the length */
				gradient_parameters[4] = sqrtf(dirv_x * dirv_x + dirv_y * dirv_y);

				/* calculate the normal */
				gradient_parameters[2] = -(dirv_y) / gradient_parameters[4];
				gradient_parameters[3] = dirv_x / gradient_parameters[4];
				return;
			}
			break;

		case VG_PAINT_RADIAL_GRADIENT:
			if(count == 5) {
				/* focal point is just copied */
				gradient_parameters[0] = values[2];
				gradient_parameters[1] = values[3];

				/* calculate the direction vector, fx' and fy' */
				gradient_parameters[2] = values[2] - values[0];
				gradient_parameters[3] = values[3] - values[1];

				/* calculate the radius, squared */
				gradient_parameters[4] = values[4] * values[4];

				/* calculate gradient denominator:
				 * 1 / (r^2 − (fx'^2 + fy'^2))
				 */
				gradient_parameters[5] =
					1.0f / (gradient_parameters[4] -
						(gradient_parameters[2] * gradient_parameters[2]
						 +
						 gradient_parameters[3] * gradient_parameters[3]
							));
				return;
			}
			break;
		}
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Paint::vgSetParameteriv(VGint paramType, VGint count, const VGint *values) {
		if(count != 1)
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		else
			vgSetParameteri(paramType, values[0]);
	}

	VGfloat Paint::vgGetParameterf(VGint paramType) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0.0f;
	}

	VGint Paint::vgGetParameteri(VGint paramType) {
		switch(paramType) {
		case VG_PAINT_TYPE:
			return (VGint)ptype;
		case VG_PAINT_COLOR_RAMP_SPREAD_MODE:
			return (VGint)spread_mode;
		case VG_PAINT_COLOR_RAMP_PREMULTIPLIED:
			return premultiplied ? VG_TRUE : VG_FALSE;
		case VG_PAINT_PATTERN_TILING_MODE:
			return (VGint)tiling_mode;
		case VG_PAINT_COLOR:
		case VG_PAINT_COLOR_RAMP_STOPS:
		case VG_PAINT_LINEAR_GRADIENT:
		case VG_PAINT_RADIAL_GRADIENT:
			break;
		}
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0;
	}

	VGint Paint::vgGetParameterVectorSize(VGint paramType) {
		switch(paramType) {
		case VG_PAINT_TYPE:
		case VG_PAINT_COLOR_RAMP_SPREAD_MODE:
		case VG_PAINT_COLOR_RAMP_PREMULTIPLIED:
		case VG_PAINT_PATTERN_TILING_MODE:
			return 1;
		case VG_PAINT_COLOR:
			return 4;
		case VG_PAINT_COLOR_RAMP_STOPS:
			return 5 * max_stops;
		case VG_PAINT_LINEAR_GRADIENT:
			return 4;
		case VG_PAINT_RADIAL_GRADIENT:
			return 5;
		}
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0;
	}

	void Paint::vgGetParameterfv(VGint paramType, VGint count, VGfloat *values) {
		switch(paramType) {
		case VG_PAINT_TYPE:
		case VG_PAINT_COLOR_RAMP_SPREAD_MODE:
		case VG_PAINT_COLOR_RAMP_PREMULTIPLIED:
		case VG_PAINT_PATTERN_TILING_MODE:
			break;
		case VG_PAINT_COLOR:
			if(count == 4) {
				for(int k = 0; k < 4; k++) {
					 values[k] = color.c[k];
				}
			} else {
				context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			}
			return;

		case VG_PAINT_COLOR_RAMP_STOPS:
			if(count > 0) {
				auto new_stops = count / 5;
				auto av_stops = new_stops < max_stops ? new_stops : max_stops;

				int k = 0; int c = 0; int v_off = 0;
				for(;
				    k < av_stops;
				    k++, c += 4, v_off += 5) {
					// get offset
					values[v_off] = color_ramp_stop_offset[k];
					// get rgba
					values[v_off + 1] = color_ramp_stop_color[c + 0];
					values[v_off + 2] = color_ramp_stop_color[c + 1];
					values[v_off + 3] = color_ramp_stop_color[c + 2];
					values[v_off + 4] = color_ramp_stop_color[c + 3];
				}
			}
			return;

		case VG_PAINT_LINEAR_GRADIENT:
			if(count == 4) {
				/* start position is just copied */
				values[0] = gradient_parameters[0];
				values[1] = gradient_parameters[1];

				auto length = gradient_parameters[4];
				auto dirv_y = -gradient_parameters[2] * length;
				auto dirv_x = gradient_parameters[3] * length;

				values[2] = dirv_x + values[0];
				values[3] = dirv_y + values[1];
				return;
			}
			break;

		case VG_PAINT_RADIAL_GRADIENT:
			if(count == 5) {
				/* focal point is just copied */
				values[2] = gradient_parameters[0];
				values[3] = gradient_parameters[1];

				values[0] = values[2] - gradient_parameters[2];
				values[1] = values[3] - gradient_parameters[3];

				values[4] = sqrtf(gradient_parameters[4]);
				return;
			}
			break;
		}
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Paint::vgGetParameteriv(VGint paramType, VGint count, VGint *values) {
		if(count != 1)
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		else
			values[0] = vgGetParameteri(paramType);
	}

	void Paint::vgPaintPattern(std::shared_ptr<Image> image) {
		pattern = image;
	}
}

using namespace gnuVG;

namespace eka2l1::dispatch {
	BRIDGE_FUNC_LIBRARY(VGPaint, vg_create_paint_emu) {
		Context *context = gnuVG::get_active_context(sys);

		if (context) {
			auto paint = context->create<Paint>();
			if(paint) return (VGPaint)paint->get_handle();
		}

		return VG_INVALID_HANDLE;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_destroy_paint_emu, VGPaint paint) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (context) {
			context->dereference(state, paint);
		}
	}

	BRIDGE_FUNC_LIBRARY(void, vg_set_paint_emu, VGPaint paint, VGbitfield paintModes) {
		Context *context = gnuVG::get_active_context(sys);

		if (context) {
			auto p = context->get<Paint>(paint);
			context->vgSetPaint(p, paintModes);
		}
	}

	BRIDGE_FUNC_LIBRARY(VGPaint, vg_get_paint_emu, VGPaintMode paintMode) {
		Context *context = gnuVG::get_active_context(sys);

		if (context) {
			auto p = context->vgGetPaint(paintMode);
			return p ? (VGPaint)p->get_handle() : VG_INVALID_HANDLE;
		}

		return VG_INVALID_HANDLE;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_set_color_emu, VGPaint paint, VGuint rgba) {
		Context *context = gnuVG::get_active_context(sys);

		if (context) {
			auto p = context->get<Paint>(paint);
			if(p)
				p->vgSetColor(rgba);
		}
	}

	BRIDGE_FUNC_LIBRARY(VGuint, vg_get_color_emu, VGPaint paint) {
		Context *context = gnuVG::get_active_context(sys);

		if (context) {
			auto p = context->get<Paint>(paint);
			return p ? p->vgGetColor() : 0;
		}

		return 0;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_paint_pattern_emu, VGPaint paint, VGImage pattern) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto p = context->get<Paint>(paint);
		auto i = (pattern == 0) ? nullptr : context->get<Image>(pattern);

		if(p)
			p->vgPaintPattern(i);
	}
}
