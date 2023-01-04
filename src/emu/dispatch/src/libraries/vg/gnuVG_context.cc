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

#include <limits>
#include <string.h>

#include <dispatch/libraries/vg/consts.h>
#include <dispatch/libraries/vg/gnuVG_context.hh>
#include <dispatch/libraries/vg/gnuVG_config.hh>
#include <dispatch/libraries/vg/gnuVG_image.hh>

#include <dispatch/libraries/vg/gnuVG_emuutils.hh>

namespace gnuVG {
	Context::Context()
		: user_matrix(GNUVG_MATRIX_PATH_USER_TO_SURFACE)
		, conversion_matrix(GNUVG_MATRIX_PATH_USER_TO_SURFACE)
		, stroke_width(1.0f)
		, stroke_dash_phase_reset(false)
		, miter_limit(4.0f)
		, join_style(VG_JOIN_MITER)
		, current_framebuffer(&screen_buffer)
		, idalloc(1)
	{
		default_fill_paint = create<Paint>();
		default_stroke_paint = create<Paint>();
		fill_paint = default_fill_paint;
		stroke_paint = default_stroke_paint;
	}

	Context::~Context() {}

	void Context::on_being_set_current() {
		for(auto k = 0; k < GNUVG_MATRIX_MAX; k++)
			matrix_is_dirty[k] = true;
	}

	void Context::set_error(VGErrorCode new_error) {
		last_error = new_error;
	}

	VGErrorCode Context::get_error() {
		auto retval = last_error;
		last_error = VG_NO_ERROR;
		return retval;
	}

	void Context::matrix_resize(VGint pxlw, VGint pxlh) {
		/* prepare screen matrix */
		VGfloat scale_w = static_cast<VGfloat>(pxlw), scale_h = static_cast<VGfloat>(pxlh);

		screen_matrix.init_identity();
		screen_matrix.translate(-1.0, -1.0);
		screen_matrix.scale(2.0, 2.0);
		screen_matrix.scale(1.0f/scale_w, 1.0f/scale_h);

		for(int k = 0; k < GNUVG_MATRIX_MAX; ++k)
			matrix_is_dirty[k] = true;
	}

	VGfloat Context::get_stroke_width() {
		return stroke_width;
	}

	VGfloat Context::get_miter_limit() {
		return miter_limit;
	}

	VGJoinStyle Context::get_join_style() {
		return join_style;
	}

	std::vector<VGfloat> Context::get_dash_pattern() {
		return dash_pattern;
	}

	VGfloat Context::get_dash_phase() {
		return stroke_dash_phase;
	}

	bool Context::get_dash_phase_reset() {
		return stroke_dash_phase_reset;
	}

	void Context::vgFlush() {
	}

	void Context::vgFinish() {
	}

	/* OpenVG equivalent API - Paint Manipulation */
	void Context::vgSetPaint(std::shared_ptr<Paint> p, VGbitfield paintModes) {
		if(!p) {
			if(paintModes & VG_FILL_PATH)
				fill_paint = default_fill_paint;
			if(paintModes & VG_STROKE_PATH)
				stroke_paint = default_stroke_paint;
		} else {
			if(paintModes & VG_FILL_PATH) {
				fill_paint = p;
			}
			if(paintModes & VG_STROKE_PATH) {
				stroke_paint = p;
			}
		}
	}

	std::shared_ptr<Paint> Context::vgGetPaint(VGbitfield paintModes) {
		std::shared_ptr<Paint> novalue;
		if(
			!(
				paintModes == VG_FILL_PATH
				||
				paintModes == VG_STROKE_PATH

				)

			){
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			return novalue;
		}
		if(paintModes == VG_FILL_PATH) {
			return fill_paint == default_fill_paint ? novalue : fill_paint;
		}
		if(paintModes == VG_STROKE_PATH) {
			return stroke_paint == default_stroke_paint ? novalue : stroke_paint;
		}

		return novalue;
	}

	/* OpenVG equivalent API - Matrix Manipulation */
	void Context::vgLoadIdentity(void) {
		matrix[user_matrix].init_identity();
		matrix_is_dirty[user_matrix] = true;
	}

	void Context::vgLoadMatrix(const VGfloat * m) {
		matrix[user_matrix].a = m[0];
		matrix[user_matrix].b = m[1];
		matrix[user_matrix].c = m[2];
		matrix[user_matrix].d = m[3];
		matrix[user_matrix].e = m[4];
		matrix[user_matrix].f = m[5];
		matrix[user_matrix].g = m[6];
		matrix[user_matrix].h = m[7];
		matrix[user_matrix].i = m[8];

		matrix_is_dirty[user_matrix] = true;
	}

	void Context::vgGetMatrix(VGfloat * m) {
		m[0] = matrix[user_matrix].a;
		m[1] = matrix[user_matrix].b;
		m[2] = matrix[user_matrix].c;
		m[3] = matrix[user_matrix].d;
		m[4] = matrix[user_matrix].e;
		m[5] = matrix[user_matrix].f;
		m[6] = matrix[user_matrix].g;
		m[7] = matrix[user_matrix].h;
		m[8] = matrix[user_matrix].i;
	}

	void Context::vgMultMatrix(const VGfloat * m) {
		Matrix b(
			m[0], m[3], m[6],
			m[1], m[4], m[7],
			m[2], m[5], m[8]
			);

		Matrix result; result.multiply(matrix[user_matrix], b);

		matrix[user_matrix].set_to(result);
		matrix_is_dirty[user_matrix] = true;
	}

	void Context::vgTranslate(VGfloat tx, VGfloat ty) {
		matrix[user_matrix].translate(tx, ty);
		matrix_is_dirty[user_matrix] = true;
	}

	void Context::vgScale(VGfloat sx, VGfloat sy) {
		matrix[user_matrix].scale(sx, sy);
		matrix_is_dirty[user_matrix] = true;
	}

	void Context::vgShear(VGfloat shx, VGfloat shy) {
		matrix[user_matrix].shear(shx, shy);
		matrix_is_dirty[user_matrix] = true;
	}

	void Context::vgRotate(VGfloat angle_in_degrees) {
		matrix[user_matrix].rotate(static_cast<VGfloat>(M_PI * angle_in_degrees) * (1.0f / 180.0f));
		matrix_is_dirty[user_matrix] = true;
	}

	void Context::reset_bounding_box() {
		bounding_box_was_reset = true;
	}

	void Context::get_bounding_box(VGfloat *sp_ep) {
		sp_ep[0] = bounding_box[0].x;
		sp_ep[1] = bounding_box[0].y;
		sp_ep[2] = bounding_box[1].x;
		sp_ep[3] = bounding_box[1].y;
	}

	Point Context::map_point(const Point &p) {
		return matrix[user_matrix].map_point(p);
	}

	/* Inherited Object API */
	void Context::vgSetf(VGint paramType, VGfloat value) {
		switch(paramType) {
			/* For the following paramType values this call is illegal */
		case VG_STROKE_DASH_PATTERN:
		case VG_STROKE_DASH_PHASE_RESET:
		case VG_MATRIX_MODE:
		case VG_CLEAR_COLOR:
		case VG_STROKE_CAP_STYLE:
		case VG_STROKE_JOIN_STYLE:
		case VG_COLOR_TRANSFORM:
		case VG_COLOR_TRANSFORM_VALUES:
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			break;
			/* Mode settings */
		case VG_FILL_RULE:
		case VG_IMAGE_QUALITY:
		case VG_RENDERING_QUALITY:
		case VG_BLEND_MODE:
		case VG_IMAGE_MODE:
			break;

			/* Color Transformation */
			break;

			/* Stroke parameters */
		case VG_STROKE_LINE_WIDTH:
			stroke_width = value;
			break;
		case VG_STROKE_DASH_PHASE:
			stroke_dash_phase = value;
			break;

		case VG_STROKE_MITER_LIMIT:
			miter_limit = value;
			if(miter_limit < 1.0f) miter_limit = 1.0f; // silently clamp small/negative values to 1.0f
			break;

			/* Edge fill color for VG_TILE_FILL tiling mode */
		case VG_TILE_FILL_COLOR:
			break;


			/* Glyph origin */
		case VG_GLYPH_ORIGIN:
			break;

			/* Pixel layout information */
		case VG_PIXEL_LAYOUT:
		case VG_SCREEN_LAYOUT:
			break;

			/* Source format selection for image filters */
		case VG_FILTER_FORMAT_LINEAR:
		case VG_FILTER_FORMAT_PREMULTIPLIED:
			break;

			/* Destination write enable mask for image filters */
		case VG_FILTER_CHANNEL_MASK:
			break;

		default:
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			break;
		}
	}

	void Context::vgSeti(const GraphicState &graphic_state, VGint paramType, VGint value) {
		switch(paramType) {
			/* For the following paramType values this call is illegal */
		default:
		case VG_STROKE_DASH_PATTERN:
		case VG_STROKE_DASH_PHASE:
		case VG_STROKE_LINE_WIDTH:
		case VG_CLEAR_COLOR:
		case VG_STROKE_MITER_LIMIT:
		case VG_COLOR_TRANSFORM_VALUES:
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			break;
			/* Mode settings */
		case VG_MATRIX_MODE:
			switch((VGMatrixMode)value) {
			case VG_MATRIX_PATH_USER_TO_SURFACE:
				user_matrix = GNUVG_MATRIX_PATH_USER_TO_SURFACE;
				break;
			case VG_MATRIX_IMAGE_USER_TO_SURFACE:
				user_matrix = GNUVG_MATRIX_IMAGE_USER_TO_SURFACE;
				break;
			case VG_MATRIX_FILL_PAINT_TO_USER:
				user_matrix = GNUVG_MATRIX_FILL_PAINT_TO_USER;
				break;
			case VG_MATRIX_STROKE_PAINT_TO_USER:
				user_matrix = GNUVG_MATRIX_STROKE_PAINT_TO_USER;
				break;
			case VG_MATRIX_GLYPH_USER_TO_SURFACE:
				user_matrix = GNUVG_MATRIX_GLYPH_USER_TO_SURFACE;
				break;
			default:
				set_error(VG_ILLEGAL_ARGUMENT_ERROR);
				break;
			}
			break;

		case VG_FILL_RULE:
		case VG_IMAGE_QUALITY:
		case VG_RENDERING_QUALITY:
		case VG_IMAGE_MODE:
			break;

		case VG_BLEND_MODE:
			switch((VGBlendMode)value) {
			case VG_BLEND_SRC:
				blend_mode = Shader::blend_src;
				break;
			case VG_BLEND_SRC_OVER:
				blend_mode = Shader::blend_src_over;
				break;
			case VG_BLEND_DST_OVER:
				blend_mode = Shader::blend_dst_over;
				break;
			case VG_BLEND_SRC_IN:
				blend_mode = Shader::blend_src_in;
				break;
			case VG_BLEND_DST_IN:
				blend_mode = Shader::blend_dst_in;
				break;
			case VG_BLEND_MULTIPLY:
				blend_mode = Shader::blend_multiply;
				break;
			case VG_BLEND_SCREEN:
				blend_mode = Shader::blend_screen;
				break;
			case VG_BLEND_DARKEN:
				blend_mode = Shader::blend_darken;
				break;
			case VG_BLEND_LIGHTEN:
				blend_mode = Shader::blend_lighten;
				break;
			case VG_BLEND_ADDITIVE:
				blend_mode = Shader::blend_additive;
				break;
			default:
				set_error(VG_ILLEGAL_ARGUMENT_ERROR);
				break;
			}

			break;

			/* Color Transformation */
		case VG_COLOR_TRANSFORM:
			do_color_transform = (((VGboolean)value) == VG_TRUE) ? true : false;
			break;

			/* Stroke parameters */
		case VG_STROKE_CAP_STYLE:
			break;
		case VG_STROKE_JOIN_STYLE:
			switch((VGJoinStyle)value) {
			case VG_JOIN_MITER:
				join_style = VG_JOIN_MITER;
				break;
			case VG_JOIN_ROUND:
				join_style = VG_JOIN_ROUND;
				break;
			case VG_JOIN_BEVEL:
				join_style = VG_JOIN_BEVEL;
				break;
			default:
				set_error(VG_ILLEGAL_ARGUMENT_ERROR);
				break;
			}
			break;
		case VG_STROKE_DASH_PHASE_RESET:
			stroke_dash_phase_reset = (((VGboolean)value) == VG_TRUE) ? true : false;
			break;

			/* Edge fill color for VG_TILE_FILL tiling mode */
		case VG_TILE_FILL_COLOR:
			break;


			/* Glyph origin */
		case VG_GLYPH_ORIGIN:
			break;

			/* Enable/disable alpha masking and scissoring */
		case VG_MASKING:
			mask_is_active = (((VGboolean)value) == VG_TRUE) ? true : false;
			break;

		case VG_SCISSORING:
			scissors_are_active = (((VGboolean)value) == VG_TRUE) ? true : false;
			render_scissors(graphic_state);
			break;

			/* Pixel layout information */
		case VG_PIXEL_LAYOUT:
		case VG_SCREEN_LAYOUT:
			break;

			/* Source format selection for image filters */
		case VG_FILTER_FORMAT_LINEAR:
		case VG_FILTER_FORMAT_PREMULTIPLIED:
			break;

			/* Destination write enable mask for image filters */
		case VG_FILTER_CHANNEL_MASK:
			break;
		}
	}

	void Context::vgSetfv(VGint paramType, VGint count, const VGfloat *values) {
		switch(paramType) {
		case VG_STROKE_DASH_PATTERN:
		{
			VGfloat total_length = 0.0f;
			dash_pattern.clear();
			for(int k = 0; k < count; k++) {
				dash_pattern.push_back(values[k]);
				total_length += values[k];
			}
			if(total_length <= 0.0f) // can't have a dash pattern of length zero
				dash_pattern.clear();
		}
		break;
		case VG_CLEAR_COLOR: /* Color for vgClear */
			if(count == 4) {
				for(int k = 0; k < 4; k++) {
					clear_color.c[k] = values[k];
				}
			} else {
				set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			}
			break;
		case VG_GLYPH_ORIGIN:
			if(count == 2)
				for(int k = 0; k < 2; ++k)
					glyph_origin[k] = values[k];
			else
				set_error(VG_ILLEGAL_ARGUMENT_ERROR);

			break;
		case VG_COLOR_TRANSFORM_VALUES:
			if(count == 8)
				for(int k = 0; k < 4; ++k) {
					color_transform_scale[k] = values[k];
					color_transform_bias[k] = values[k + 4];
				}
			else
				set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		default:
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		}
	}

	void Context::vgSetiv(const GraphicState &state, VGint paramType, VGint count, const VGint *values) {
		switch(paramType) {
			/* Scissoring rectangles */
		case VG_SCISSOR_RECTS:
		{
			count = count > (GNUVG_MAX_SCISSORS * 4) ? (GNUVG_MAX_SCISSORS * 4) : count;

			std::int32_t l = 0, max_k = count / 4;
			for(int k = 0; k < max_k; k++) {
				int soffset = k * 4;

				float x = (float)(values[soffset + 0]);
				float y = (float)(values[soffset + 1]);
				float w = (float)(values[soffset + 2]);
				float h = (float)(values[soffset + 3]);

				// we ignore zero size rectangles
				if(w != 0.0f && h != 0.0f) {
					int vcoffset = (l * 4) << 1;
					int toffset = l * 3 * 2;
					l++;

					scissor_vertices[vcoffset + 0] = x;
					scissor_vertices[vcoffset + 1] = y;
					scissor_vertices[vcoffset + 2] = x + w;
					scissor_vertices[vcoffset + 3] = y;
					scissor_vertices[vcoffset + 4] = x + w;
					scissor_vertices[vcoffset + 5] = y + h;
					scissor_vertices[vcoffset + 6] = x;
					scissor_vertices[vcoffset + 7] = y + h;

					scissor_triangles[toffset + 0] = soffset + 0;
					scissor_triangles[toffset + 1] = soffset + 1;
					scissor_triangles[toffset + 2] = soffset + 2;
					scissor_triangles[toffset + 3] = soffset + 2;
					scissor_triangles[toffset + 4] = soffset + 3;
					scissor_triangles[toffset + 5] = soffset + 0;
				}
			}
			nr_active_scissors = l;

			render_scissors(state);
		}
		break;

		default:
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			break;
		}
	}

	VGfloat Context::vgGetf(VGint paramType) {
		switch(paramType) {
			/* For the following paramType values this call is illegal */
		case VG_STROKE_DASH_PATTERN:
		case VG_STROKE_DASH_PHASE_RESET:
		case VG_MATRIX_MODE:
		case VG_CLEAR_COLOR:
		case VG_STROKE_CAP_STYLE:
		case VG_STROKE_JOIN_STYLE:
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			break;
			/* Mode settings */
		case VG_FILL_RULE:
		case VG_IMAGE_QUALITY:
		case VG_RENDERING_QUALITY:
		case VG_BLEND_MODE:
		case VG_IMAGE_MODE:
			break;

			/* Scissoring rectangles */
		case VG_SCISSOR_RECTS:
			break;

			/* Color Transformation */
		case VG_COLOR_TRANSFORM:
		case VG_COLOR_TRANSFORM_VALUES:
			break;

			/* Stroke parameters */
		case VG_STROKE_LINE_WIDTH:
			return stroke_width;
		case VG_STROKE_DASH_PHASE:
			return stroke_dash_phase;

		case VG_STROKE_MITER_LIMIT:
			return miter_limit;

			/* Edge fill color for VG_TILE_FILL tiling mode */
		case VG_TILE_FILL_COLOR:
			break;

			/* Enable/disable alpha masking and scissoring */
		case VG_MASKING:
		case VG_SCISSORING:
			break;

			/* Pixel layout information */
		case VG_PIXEL_LAYOUT:
		case VG_SCREEN_LAYOUT:
			break;

			/* Source format selection for image filters */
		case VG_FILTER_FORMAT_LINEAR:
		case VG_FILTER_FORMAT_PREMULTIPLIED:
			break;

			/* Destination write enable mask for image filters */
		case VG_FILTER_CHANNEL_MASK:
			break;

			/* Implementation limits (read-only) */
		case VG_MAX_FLOAT:
			return std::numeric_limits<VGfloat>::max();

		case VG_MAX_DASH_COUNT:
		case VG_MAX_KERNEL_SIZE:
		case VG_MAX_SEPARABLE_KERNEL_SIZE:
		case VG_MAX_IMAGE_WIDTH:
		case VG_MAX_IMAGE_HEIGHT:
		case VG_MAX_IMAGE_PIXELS:
		case VG_MAX_IMAGE_BYTES:
		case VG_MAX_GAUSSIAN_STD_DEVIATION:
			break;
		}

		// illegal argument (/not implemented)
		set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0.0f;
	}

	VGint Context::vgGeti(VGint paramType) {
		switch(paramType) {
			/* For the following paramType values this call is illegal */
		default:
		case VG_STROKE_DASH_PATTERN:
		case VG_STROKE_DASH_PHASE:
		case VG_STROKE_LINE_WIDTH:
		case VG_CLEAR_COLOR:
		case VG_STROKE_MITER_LIMIT:
		case VG_COLOR_TRANSFORM_VALUES:
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			break;
			/* Mode settings */
		case VG_MATRIX_MODE:
		{
			return user_matrix;
		}

		case VG_FILL_RULE:
		case VG_IMAGE_QUALITY:
		case VG_RENDERING_QUALITY:
		case VG_IMAGE_MODE:
			break;

		case VG_BLEND_MODE:
			switch(blend_mode) {
			case Shader::blend_src:
				return VG_BLEND_SRC;
			case Shader::blend_src_over:
				return VG_BLEND_SRC_OVER;
			case Shader::blend_dst_over:
				return VG_BLEND_DST_OVER;
			case Shader::blend_src_in:
				return VG_BLEND_SRC_IN;
			case Shader::blend_dst_in:
				return VG_BLEND_DST_IN;
			case Shader::blend_multiply:
				return VG_BLEND_MULTIPLY;
			case Shader::blend_screen:
				return VG_BLEND_SCREEN;
			case Shader::blend_darken:
				return VG_BLEND_DARKEN;
			case Shader::blend_lighten:
				return VG_BLEND_LIGHTEN;
			case Shader::blend_additive:
				return VG_BLEND_ADDITIVE;
			}
			break;

			/* Scissoring rectangles */
		case VG_SCISSOR_RECTS:
			break;

			/* Color Transformation */
		case VG_COLOR_TRANSFORM:
			return do_color_transform ? VG_TRUE : VG_FALSE;
			break;

			/* Stroke parameters */
		case VG_STROKE_CAP_STYLE:
			break;
		case VG_STROKE_JOIN_STYLE:
			return join_style;
		case VG_STROKE_DASH_PHASE_RESET:
			return stroke_dash_phase_reset ? VG_TRUE : VG_FALSE;

			/* Edge fill color for VG_TILE_FILL tiling mode */
		case VG_TILE_FILL_COLOR:
			break;


			/* Glyph origin */
		case VG_GLYPH_ORIGIN:
			break;

			/* Enable/disable alpha masking and scissoring */
		case VG_MASKING:
			return mask_is_active ? VG_TRUE : VG_FALSE;

		case VG_SCISSORING:
			return scissors_are_active ? VG_TRUE : VG_FALSE;

			/* Pixel layout information */
		case VG_PIXEL_LAYOUT:
		case VG_SCREEN_LAYOUT:
			break;

			/* Source format selection for image filters */
		case VG_FILTER_FORMAT_LINEAR:
		case VG_FILTER_FORMAT_PREMULTIPLIED:
			break;

			/* Destination write enable mask for image filters */
		case VG_FILTER_CHANNEL_MASK:
			break;

			/* Implementation limits (read-only) */
		case VG_MAX_SCISSOR_RECTS:
			return GNUVG_MAX_SCISSORS;
			break;

		case VG_MAX_COLOR_RAMP_STOPS:
			return GNUVG_MAX_COLOR_RAMP_STOPS;

		case VG_MAX_KERNEL_SIZE:
		case VG_MAX_SEPARABLE_KERNEL_SIZE:
		case VG_MAX_IMAGE_WIDTH:
		case VG_MAX_IMAGE_HEIGHT:
		case VG_MAX_IMAGE_PIXELS:
		case VG_MAX_IMAGE_BYTES:
		case VG_MAX_FLOAT:
		case VG_MAX_GAUSSIAN_STD_DEVIATION:
			break;
		case VG_MAX_DASH_COUNT:
			return 128;
		}

		// illegal argument (/not implemented)
		set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return -1; // not implemented
	}

	VGint Context::vgGetVectorSize(VGint paramType) {
		switch(paramType) {
		case VG_MATRIX_MODE:
		case VG_FILL_RULE:
		case VG_IMAGE_QUALITY:
		case VG_RENDERING_QUALITY:
		case VG_BLEND_MODE:
		case VG_IMAGE_MODE:
		case VG_COLOR_TRANSFORM:
		case VG_STROKE_LINE_WIDTH:
		case VG_STROKE_CAP_STYLE:
		case VG_STROKE_JOIN_STYLE:
		case VG_STROKE_MITER_LIMIT:
		case VG_STROKE_DASH_PHASE:
		case VG_STROKE_DASH_PHASE_RESET:
		case VG_MASKING:
		case VG_SCISSORING:
		case VG_SCREEN_LAYOUT:
		case VG_PIXEL_LAYOUT:
		case VG_FILTER_FORMAT_LINEAR:
		case VG_FILTER_FORMAT_PREMULTIPLIED:
		case VG_FILTER_CHANNEL_MASK:
		case VG_MAX_SCISSOR_RECTS:
		case VG_MAX_DASH_COUNT:
		case VG_MAX_KERNEL_SIZE:
		case VG_MAX_SEPARABLE_KERNEL_SIZE:
		case VG_MAX_GAUSSIAN_STD_DEVIATION:
		case VG_MAX_COLOR_RAMP_STOPS:
		case VG_MAX_IMAGE_WIDTH:
		case VG_MAX_IMAGE_HEIGHT:
		case VG_MAX_IMAGE_PIXELS:
		case VG_MAX_IMAGE_BYTES:
		case VG_MAX_FLOAT:
			return 1;

		case VG_SCISSOR_RECTS:
			return nr_active_scissors * 4;

		case VG_COLOR_TRANSFORM_VALUES:
			return 8;

		case VG_STROKE_DASH_PATTERN:
			return (VGint)dash_pattern.size();

		case VG_TILE_FILL_COLOR:
		case VG_CLEAR_COLOR:
			return 4;

		case VG_GLYPH_ORIGIN:
			return 2;
		}
		return 0;
	}

	void Context::vgGetfv(VGint paramType, VGint count, VGfloat *values) {
		switch(paramType) {
		case VG_GLYPH_ORIGIN:
			if(count == 2)
				for(int k = 0; k < 2; ++k)
					values[k] = glyph_origin[k];
			break;
		case VG_COLOR_TRANSFORM_VALUES:
			if(count == 8)
				for(int k = 0; k < 4; ++k) {
					values[k    ] = color_transform_scale[k];
					values[k + 4] = color_transform_bias[k];
				}
			break;
		default:
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		}
	}

	void Context::vgGetiv(VGint paramType, VGint count, VGint *values) {
	}

}

/* Backend implementation specific functions - OpenGL */
namespace gnuVG {

	void Context::render_scissors(const GraphicState &state) {
		if(scissors_are_active &&  nr_active_scissors > 0) {
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, true);
			cmd_builder_.set_color_mask(0);

			// first we clear the stencil to zero
			cmd_builder_.set_stencil_mask(eka2l1::drivers::rendering_face::back_and_front, 0xFF);
			cmd_builder_.clear({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, eka2l1::drivers::draw_buffer_bit_stencil_buffer);

			// then we render the scissor elements
			cmd_builder_.set_stencil_pass_condition(eka2l1::drivers::rendering_face::back_and_front,
				eka2l1::drivers::condition_func::always, 1, 1);
			cmd_builder_.set_stencil_action(eka2l1::drivers::rendering_face::back_and_front,
				eka2l1::drivers::stencil_action::replace,
				eka2l1::drivers::stencil_action::replace,
				eka2l1::drivers::stencil_action::replace);

			trivial_render_elements(state,
						scissor_vertices,
						8 * nr_active_scissors,
						scissor_triangles,
						6 * nr_active_scissors,
						// colors will be ignored
						// because of disabled color mask
						1.0, 0.0, 0.0, 0.5);

			cmd_builder_.set_stencil_pass_condition(eka2l1::drivers::rendering_face::back_and_front,
				eka2l1::drivers::condition_func::equal, 1, 1);
			cmd_builder_.set_stencil_action(eka2l1::drivers::rendering_face::back_and_front,
				eka2l1::drivers::stencil_action::keep,
				eka2l1::drivers::stencil_action::keep,
				eka2l1::drivers::stencil_action::keep);
			cmd_builder_.set_color_mask(0b1111);
			cmd_builder_.set_stencil_mask(eka2l1::drivers::rendering_face::back_and_front, 0);
		} else {
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);
		}
	}

	void Context::setup_buffers_and_descriptors(
		eka2l1::drivers::graphics_driver *driver,
		const float *vertex2d, 
		const std::size_t vertex_buffer_size,
		const std::int32_t vertex_stride,
		const float *texcoord2d,
		const std::size_t texcoord_buffer_size,
		const std::int32_t texcoord_stride) {
		eka2l1::drivers::input_descriptor descriptor[2];
		eka2l1::drivers::handle buffers[2];

		if (!vertex2d) {
			GNUVG_ERROR("No vertex buffer data to setup!");
			return;
		}

		std::size_t offset = 0;
		buffers[0] = vertex_buffer_pusher.push_buffer(driver, reinterpret_cast<const std::uint8_t*>(vertex2d),
			vertex_buffer_size, offset);

		descriptor[0].buffer_slot = 0;
		descriptor[0].set_normalized(false);
		descriptor[0].set_format(2, eka2l1::drivers::data_format::sfloat);
		descriptor[0].offset = static_cast<int>(offset);
		descriptor[0].location = Shader::attrib_location_position;

		if (!texcoord2d) {
			if (!input_desc) {
				input_desc = eka2l1::drivers::create_input_descriptors(driver, descriptor, 1);
			} else {
				cmd_builder_.update_input_descriptors(input_desc, descriptor, 1);
			}

			cmd_builder_.set_vertex_buffers(buffers, 0, 1);
			cmd_builder_.bind_input_descriptors(input_desc);

			return;
		}

		eka2l1::drivers::handle texcoord_buffer_handle = vertex_buffer_pusher.push_buffer(driver,
			reinterpret_cast<const std::uint8_t*>(texcoord2d), texcoord_buffer_size, offset);

		if (texcoord_buffer_handle == buffers[0]) {
			descriptor[1].buffer_slot = 0;
		} else {
			descriptor[1].buffer_slot = 1;
			buffers[1] = texcoord_buffer_handle;
		}
		
		descriptor[1].set_normalized(false);
		descriptor[1].set_format(2, eka2l1::drivers::data_format::sfloat);
		descriptor[1].offset = static_cast<int>(offset);
		descriptor[1].location = Shader::attrib_location_texcoord;
	
		if (!input_desc) {
			input_desc = eka2l1::drivers::create_input_descriptors(driver, descriptor, 2);
		} else {
			cmd_builder_.update_input_descriptors(input_desc, descriptor, 2);
		}

		cmd_builder_.set_vertex_buffers(buffers, 0, (descriptor[1].buffer_slot == 0) ? 1 : 2);
		cmd_builder_.bind_input_descriptors(input_desc);
	}

	void Context::recreate_buffers(const GraphicState &state) {
		// Then the mask
		if(mask.framebuffer != 0)
			delete_framebuffer(state, &mask);
		if(!create_framebuffer(state, &mask, VG_sRGBA_8888,
				       current_framebuffer->width, current_framebuffer->height,
				       VG_IMAGE_QUALITY_FASTER))
			GNUVG_ERROR("failed to create framebuffer for mask.\n");

		// Then two temporary buffers
		if(temporary_a.framebuffer != 0)
			delete_framebuffer(state, &temporary_a);
		if(!create_framebuffer(state, &temporary_a, VG_sRGBA_8888,
				       current_framebuffer->width, current_framebuffer->height,
				       VG_IMAGE_QUALITY_FASTER))
			GNUVG_ERROR("failed to create framebuffer for temporary_a.\n");
		if(temporary_b.framebuffer != 0)
			delete_framebuffer(state, &temporary_b);
		if(!create_framebuffer(state, &temporary_b, VG_sRGBA_8888,
				       current_framebuffer->width, current_framebuffer->height,
				       VG_IMAGE_QUALITY_FASTER))
			GNUVG_ERROR("failed to create framebuffer for temporary_b.\n");
	}

	void Context::resize(const GraphicState &state, VGint pxlw, VGint pxlh) {
		screen_buffer.width = pxlw;
		screen_buffer.height = pxlh;

		if(current_framebuffer == &screen_buffer)
			render_to_framebuffer(state, &screen_buffer);
	}

	void Context::clear(const GraphicState &state, VGint x, VGint y,
			    VGint width, VGint height) {

		// remember current blend mode
		auto old_blend = vgGeti(VG_BLEND_MODE);

		// fill the area with the clear color, using no blending
		vgSeti(state, VG_BLEND_MODE, VG_BLEND_SRC);
		trivial_fill_area(state, x, y, width, height,
				  clear_color.r,
				  clear_color.g,
				  clear_color.b,
				  clear_color.a);

		vgSeti(state, VG_BLEND_MODE, old_blend); // Restore blend mode
	}

	void Context::trivial_render_framebuffer(const GraphicState &state,
						 const FrameBuffer* framebuffer,
						 int gaussian_width,
						 int gaussian_height,
						 VGTilingMode tiling_mode) {
		VGfloat w, h;

		if(framebuffer->subset_width >= 0) {
			w = (VGfloat)(framebuffer->subset_width);
			h = (VGfloat)(framebuffer->subset_height);
		} else {
			w = (VGfloat)(framebuffer->width);
			h = (VGfloat)(framebuffer->height);
		}

		// calculate corners of image
		auto c1_p = matrix[GNUVG_MATRIX_IMAGE_USER_TO_SURFACE].map_point(Point(0.0f, 0.0f));
		auto c2_p = matrix[GNUVG_MATRIX_IMAGE_USER_TO_SURFACE].map_point(Point(0.0f,    h));
		auto c3_p = matrix[GNUVG_MATRIX_IMAGE_USER_TO_SURFACE].map_point(Point(   w,    h));
		auto c4_p = matrix[GNUVG_MATRIX_IMAGE_USER_TO_SURFACE].map_point(Point(   w, 0.0f));

		Point
			c1(-1.0, -1.0),
			c2(-1.0,  1.0),
			c3( 1.0,  1.0),
			c4( 1.0, -1.0);

		float vertices_full[] = {
			c1.x, c1.y,
			c2.x, c2.y,
			c3.x, c3.y,
			c4.x, c4.y,
		};

		if(tiling_mode == VG_TILE_FILL) {
			c1 = screen_matrix.map_point(c1_p);
			c2 = screen_matrix.map_point(c2_p);
			c3 = screen_matrix.map_point(c3_p);
			c4 = screen_matrix.map_point(c4_p);
		}

		float mat[] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};

		float mat_full[] = {
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		};

		float vertices[] = {
			c1.x, c1.y,
			c2.x, c2.y,
			c3.x, c3.y,
			c4.x, c4.y,
		};

		std::uint32_t indices[] = {
			0, 1, 2,
			0, 2, 3
		};

		eka2l1::drivers::addressing_option wrap_mode = eka2l1::drivers::addressing_option::clamp_to_edge;
		switch(tiling_mode) {
		default:
		case VG_TILING_MODE_FORCE_SIZE:
		case VG_TILE_FILL:
		case VG_TILE_PAD:
			wrap_mode = eka2l1::drivers::addressing_option::clamp_to_edge;
			break;
		case VG_TILE_REPEAT:
			wrap_mode = eka2l1::drivers::addressing_option::repeat;
			break;
		case VG_TILE_REFLECT:
			wrap_mode = eka2l1::drivers::addressing_option::mirrored_repeat;
			break;
		}

		prepare_framebuffer_matrix(framebuffer);

		auto f = [this, vertices, vertices_full, mat, mat_full, indices, wrap_mode, state]
			(const FrameBuffer *fbuffer,
			 bool do_horizontal_gauss,
			 int kern_diameter) {
			int caps = Shader::do_pattern;
			if(kern_diameter > 1) {
				caps |= do_horizontal_gauss ?
					Shader::do_horizontal_gaussian :
					Shader::do_vertical_gaussian;
				caps |= (kern_diameter << Shader::gauss_krn_diameter_shift) &
					Shader::gauss_krn_diameter_mask;
			}
			if(do_color_transform)
				caps |= Shader::do_color_transform;

			active_shader = state.shaderman->get_shader(
				state.driver,
				caps
				);
			active_shader->set_current_graphics_command_builder(cmd_builder_);
			active_shader->use_shader();
			active_shader->set_blending(blend_mode);
			active_shader->set_matrix(mat);

			if(do_color_transform)
				active_shader->set_color_transform(
					color_transform_scale,
					color_transform_bias);

			active_shader->set_pattern_texture(fbuffer->framebuffer, wrap_mode);
			active_shader->set_pattern_matrix(do_horizontal_gauss ?
							  mat_full :
							  image_matrix_data);
			active_shader->set_pattern_size(fbuffer->width, fbuffer->height);

			setup_buffers_and_descriptors(state.driver, do_horizontal_gauss ? vertices_full : vertices,
				8 * sizeof(float), 0, nullptr, 0, 0);

			std::size_t indices_offset = 0;
			eka2l1::drivers::handle index_buffer_handle = index_buffer_pusher.push_buffer(state.driver,
				reinterpret_cast<const std::uint8_t*>(indices), 6 * sizeof(std::uint32_t), indices_offset);

			active_shader->render_elements(index_buffer_handle, indices_offset, 6);
		};
		if(gaussian_width <= 1 && gaussian_height <= 1)
			f(framebuffer, false, 1);
		else {
			auto tfbf = get_temporary_framebuffer(state, VG_sRGBA_8888,
							      framebuffer->width, framebuffer->height,
							      VG_IMAGE_QUALITY_BETTER);

			if(tfbf) {
				save_current_framebuffer();
				render_to_framebuffer(state, tfbf);
				auto keep_old_blend_mode = blend_mode;
				blend_mode = Shader::blend_src;
				trivial_fill_area(state, 0, 0,
						  tfbf->width, tfbf->height,
						  0.0, 0.0, 0.0, 0.0);
				blend_mode = keep_old_blend_mode;
				f(framebuffer, true, gaussian_width);
				restore_current_framebuffer(state);
				f(tfbf, false, gaussian_height);
				return_temporary_framebuffer(tfbf);
			}
		}
	}

	void Context::trivial_render_elements(const GraphicState &state,
		float *vertices, std::int32_t vertices_count,
		std::uint32_t *indices, std::int32_t indices_count,
		VGfloat r, VGfloat g, VGfloat b, VGfloat a) {

		Matrix *m = &screen_matrix;
		float mat[] = {
			m->a, m->b, 0.0f, m->c,
			m->d, m->e, 0.0f, m->f,
			0.0f, 0.0f, 1.0f, 0.0f,
			m->g, m->h, 0.0f, m->i
		};

		VGfloat col[] = {r, g, b, a};

		active_shader = state.shaderman->get_shader(state.driver,
			Shader::do_flat_color |
			(do_color_transform ? Shader::do_color_transform : 0)
			);
		active_shader->use_shader();
		active_shader->set_blending(blend_mode);
		active_shader->set_matrix(mat);
		active_shader->set_color(col);
		if(do_color_transform)
			active_shader->set_color_transform(
				color_transform_scale,
				color_transform_bias);
		
		setup_buffers_and_descriptors(state.driver, vertices, vertices_count * sizeof(float), 0,
			nullptr, 0, 0);

		std::size_t indices_offset = 0;
		eka2l1::drivers::handle index_buffer_handle = index_buffer_pusher.push_buffer(state.driver,
			reinterpret_cast<const std::uint8_t*>(indices), indices_count * sizeof(std::uint32_t), indices_offset);

		active_shader->render_elements(index_buffer_handle, indices_offset, indices_count);
	}

	void Context::render_texture_alpha_triangle_array(const GraphicState &state,
							  const FrameBuffer *fb,
							  const float *ver_c_2d, const std::uint32_t ver_c_count, std::int32_t ver_stride_2d,
							  const float *tex_c_2d, const std::uint32_t tex_c_count, std::int32_t tex_stride_2d,
							  const std::uint32_t *indices, std::int32_t nr_indices,
							  const float *texture_matrix_3by3) {

		active_shader = state.shaderman->get_shader(
			state.driver,
			Shader::do_flat_color |
			(do_color_transform ? Shader::do_color_transform : 0)
			| Shader::do_texture_alpha
			);
		active_shader->set_current_graphics_command_builder(cmd_builder_);
		active_shader->use_shader();
		active_shader->set_blending(blend_mode);
		active_shader->set_matrix(conversion_matrix_data);
		if(do_color_transform)
			active_shader->set_color_transform(
				color_transform_scale,
				color_transform_bias);
		setup_buffers_and_descriptors(state.driver, ver_c_2d, ver_c_count * sizeof(float),
			ver_stride_2d, tex_c_2d, tex_c_count * sizeof(float), tex_stride_2d);
		active_shader->set_texture(fb->framebuffer);
		active_shader->set_texture_matrix(texture_matrix_3by3);
		active_shader->set_color(fill_paint->color.c);

		std::size_t indices_offset = 0;
		eka2l1::drivers::handle index_buffer_handle = index_buffer_pusher.push_buffer(state.driver,
			reinterpret_cast<const std::uint8_t*>(indices), nr_indices * sizeof(std::uint32_t), indices_offset);

		active_shader->render_elements(index_buffer_handle, indices_offset, nr_indices);
	}

	void Context::trivial_fill_area(const GraphicState &state,
		VGint _x, VGint _y, VGint _width, VGint _height,
		VGfloat r, VGfloat g, VGfloat b, VGfloat a) {

		VGfloat x = (VGfloat)_x;
		VGfloat y = (VGfloat)_y;
		VGfloat width = (VGfloat)_width;
		VGfloat height = (VGfloat)_height;

		float vertices[] = {
			x,         y,
			x + width, y,
			x + width, y + height,
			x        , y + height
		};

		std::uint32_t indices[] = {
			0, 1, 2,
			0, 2, 3
		};

		trivial_render_elements(state, vertices, 8, indices, 6, r, g, b, a);
	}

	void Context::prepare_framebuffer_matrix(const FrameBuffer* fbuf) {
		/* if dirty - calculate final matrix */
		if(matrix_is_dirty[GNUVG_MATRIX_IMAGE_USER_TO_SURFACE]) {
			matrix_is_dirty[GNUVG_MATRIX_IMAGE_USER_TO_SURFACE] = false;

			final_matrix[GNUVG_MATRIX_IMAGE_USER_TO_SURFACE].multiply(
				screen_matrix,
				matrix[GNUVG_MATRIX_IMAGE_USER_TO_SURFACE]);
		}

		/* Create matrix to convert from final on-screen OpenGL
		 * coordinates back to texture space coordinates.
		 */
		Matrix temp_a, temp_b;
		if(fbuf->subset_x >= 0)
			temp_a.translate(-fbuf->subset_x, -fbuf->subset_y);
		temp_a.scale(fbuf->width, fbuf->height);
		temp_b.multiply(final_matrix[GNUVG_MATRIX_IMAGE_USER_TO_SURFACE], temp_a);
		temp_b.invert();

		/* Convert to OpenGL friendly format */
		float mat[] = {
			temp_b.a, temp_b.b, 0.0f, temp_b.c,
			temp_b.d, temp_b.e, 0.0f, temp_b.f,
			0.0f,     0.0f,     1.0f, 0.0f,
			temp_b.g, temp_b.h, 0.0f, temp_b.i
		};

		memcpy(image_matrix_data, mat, sizeof(mat));
	}

	void Context::select_conversion_matrix(MatrixMode _conversion_matrix) {
		if(conversion_matrix != _conversion_matrix ||
		   matrix_is_dirty[_conversion_matrix] ||
		   matrix_is_dirty[GNUVG_MATRIX_FILL_PAINT_TO_USER] ||
		   matrix_is_dirty[GNUVG_MATRIX_STROKE_PAINT_TO_USER]) {

			matrix_is_dirty[_conversion_matrix] = false;
			matrix_is_dirty[GNUVG_MATRIX_FILL_PAINT_TO_USER] = false;
			matrix_is_dirty[GNUVG_MATRIX_STROKE_PAINT_TO_USER] = false;

			/* calculate regualar conversion matrix */
			final_matrix[_conversion_matrix].multiply(
				screen_matrix,
				matrix[_conversion_matrix]);

			/* then also the surface -> paint matrices */
			final_matrix[GNUVG_MATRIX_FILL_PAINT_TO_USER].multiply(
				final_matrix[_conversion_matrix],
				matrix[GNUVG_MATRIX_FILL_PAINT_TO_USER]
				);
			final_matrix[GNUVG_MATRIX_FILL_PAINT_TO_USER].invert();

			final_matrix[GNUVG_MATRIX_STROKE_PAINT_TO_USER].multiply(
				final_matrix[_conversion_matrix],
				matrix[GNUVG_MATRIX_STROKE_PAINT_TO_USER]
				);
			final_matrix[GNUVG_MATRIX_STROKE_PAINT_TO_USER].invert();
		}

		conversion_matrix = _conversion_matrix;

		/* convert user2surface conversion matrix to GL friendly format */
		{
			Matrix *m = &final_matrix[conversion_matrix];
			float mat[] = {
				m->a, m->b, 0.0f, 0.0f,
				m->d, m->e, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				m->g, m->h, 0.0f, 1.0f
			};
			memcpy(conversion_matrix_data, mat, sizeof(mat));
		}
		/* convert surface2fill conversion matrix to GL friendly format */
		{
			Matrix *m = &final_matrix[GNUVG_MATRIX_FILL_PAINT_TO_USER];
			float mat[] = {
				m->a, m->b, 0.0f, 0.0f,
				m->d, m->e, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				m->g, m->h, 0.0f, 1.0f
			};
			memcpy(surf2fill_matrix_data, mat, sizeof(mat));
		}
		/* convert surface2stroke conversion matrix to GL friendly format */
		{
			Matrix *m = &final_matrix[GNUVG_MATRIX_STROKE_PAINT_TO_USER];
			float mat[] = {
				m->a, m->b, 0.0f, 0.0f,
				m->d, m->e, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				m->g, m->h, 0.0f, 1.0f
			};
			memcpy(surf2stroke_matrix_data, mat, sizeof(mat));
		}
	}

	void Context::use_pipeline(const GraphicState &state, gnuVGPipeline new_pipeline, VGPaintMode _mode) {
		pipeline_mode = _mode;
		active_pipeline = new_pipeline;

		auto active_paint =
			pipeline_mode == VG_STROKE_PATH ? stroke_paint : fill_paint;
		int caps = Shader::do_flat_color | Shader::do_pretranslate;

		bool gradient_enabled = false;
		switch(active_paint->ptype) {
		case VG_PAINT_TYPE_COLOR:
		case VG_PAINT_TYPE_FORCE_SIZE:
			break;

		case VG_PAINT_TYPE_PATTERN:
			caps |= Shader::do_pattern;
			break;

		case VG_PAINT_TYPE_LINEAR_GRADIENT:
			caps |= Shader::do_linear_gradient;
			gradient_enabled = true;
			break;

		case VG_PAINT_TYPE_RADIAL_GRADIENT:
			caps |= Shader::do_radial_gradient;
			gradient_enabled = true;
			break;
		}

		if(mask_is_active) caps |= Shader::do_mask;

		if(gradient_enabled)
			switch(active_paint->spread_mode) {
			case VG_COLOR_RAMP_SPREAD_MODE_FORCE_SIZE:
				break;
			case VG_COLOR_RAMP_SPREAD_PAD:
				caps |= Shader::do_gradient_pad;
				break;
			case VG_COLOR_RAMP_SPREAD_REPEAT:
				caps |= Shader::do_gradient_repeat;
				break;
			case VG_COLOR_RAMP_SPREAD_REFLECT:
				caps |= Shader::do_gradient_reflect;
				break;
			}

		if(do_color_transform)
			caps |= Shader::do_color_transform;

		active_shader = state.shaderman->get_shader(state.driver, caps);
		active_shader->set_current_graphics_command_builder(cmd_builder_);
		active_shader->use_shader();
		active_shader->set_blending(blend_mode);
		active_shader->set_matrix(conversion_matrix_data);
		active_shader->set_pre_translation(pre_translation);

		if(do_color_transform)
			active_shader->set_color_transform(
				color_transform_scale,
				color_transform_bias);

		switch(pipeline_mode) {
		case VG_COLOR_RAMP_SPREAD_MODE_FORCE_SIZE:
			break;
		case VG_FILL_PATH:
			active_shader->set_surf2paint_matrix(surf2fill_matrix_data);
			break;
		case VG_STROKE_PATH:
			active_shader->set_surf2paint_matrix(surf2stroke_matrix_data);
			break;
		}

		if(mask_is_active) active_shader->set_mask_texture(mask.framebuffer);

		if(scissors_are_active) {
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, true);
			cmd_builder_.set_stencil_mask(eka2l1::drivers::rendering_face::back_and_front, 0x00);
			cmd_builder_.set_stencil_pass_condition(eka2l1::drivers::rendering_face::back_and_front,
				eka2l1::drivers::condition_func::equal, 1, 0xFF);
			cmd_builder_.set_stencil_action(eka2l1::drivers::rendering_face::back_and_front,
				eka2l1::drivers::stencil_action::keep,
				eka2l1::drivers::stencil_action::keep,
				eka2l1::drivers::stencil_action::keep);
		} else {
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);
			cmd_builder_.set_stencil_pass_condition(eka2l1::drivers::rendering_face::back_and_front,
				eka2l1::drivers::condition_func::always, 1, 1);
			cmd_builder_.set_stencil_action(eka2l1::drivers::rendering_face::back_and_front,
				eka2l1::drivers::stencil_action::keep,
				eka2l1::drivers::stencil_action::keep,
				eka2l1::drivers::stencil_action::keep);
		}

		switch(active_paint->ptype) {
		case VG_PAINT_TYPE_FORCE_SIZE:
			break;

		case VG_PAINT_TYPE_PATTERN:
			if(active_paint->pattern) {
				auto ptf = active_paint->pattern->get_framebuffer();
				prepare_framebuffer_matrix(ptf);

				eka2l1::drivers::addressing_option wrap_mode = eka2l1::drivers::addressing_option::clamp_to_edge;
				switch (active_paint->tiling_mode) {
				default:
				case VG_TILING_MODE_FORCE_SIZE:
				case VG_TILE_FILL:
				case VG_TILE_PAD:
					wrap_mode = eka2l1::drivers::addressing_option::clamp_to_edge;
					break;
				case VG_TILE_REPEAT:
					wrap_mode = eka2l1::drivers::addressing_option::repeat;
					break;
				case VG_TILE_REFLECT:
					wrap_mode = eka2l1::drivers::addressing_option::mirrored_repeat;
					break;
				}

				active_shader->set_pattern_texture(ptf->framebuffer, wrap_mode);
				active_shader->set_pattern_matrix(image_matrix_data);
			}
			break;

		case VG_PAINT_TYPE_COLOR:
			active_shader->set_color(active_paint->color.c);
			return;

		case VG_PAINT_TYPE_LINEAR_GRADIENT:
			active_shader->set_linear_parameters(active_paint->gradient_parameters);
			active_shader->set_color_ramp(
				active_paint->max_stops,
				active_paint->color_ramp_stop_offset,
				active_paint->color_ramp_stop_invfactor,
				active_paint->color_ramp_stop_color);
			break;

		case VG_PAINT_TYPE_RADIAL_GRADIENT:
			active_shader->set_radial_parameters(active_paint->gradient_parameters);
			active_shader->set_color_ramp(
				active_paint->max_stops,
				active_paint->color_ramp_stop_offset,
				active_paint->color_ramp_stop_invfactor,
				active_paint->color_ramp_stop_color);
			break;
		}
	}

	void Context::reset_pre_translation() {
		for(int k = 0; k < 2; ++k)
			pre_translation[k] = 0.0f;
	}

	void Context::use_glyph_origin_as_pre_translation(VGfloat specific_glyph_origin[2]) {
		for(int k = 0; k < 2; ++k)
			pre_translation[k] = glyph_origin[k] - specific_glyph_origin[k];
	}

	void Context::adjust_glyph_origin(VGfloat escapement[2]) {
		for(int k = 0; k < 2; ++k)
			glyph_origin[k] += escapement[k];
	}

	void Context::render_triangles(std::int32_t first, std::int32_t count) {
		if (active_shader) {
			active_shader->render_triangles(first, count);
			return;
		}
	}

	void Context::render_elements(const GraphicState &state, const std::uint32_t *indices, std::int32_t nr_indices) {
		if(active_shader) {
			std::size_t indices_offset = 0;
			eka2l1::drivers::handle index_buffer_handle = index_buffer_pusher.push_buffer(state.driver,
				reinterpret_cast<const std::uint8_t*>(indices), 6 * sizeof(std::uint32_t), indices_offset);

			active_shader->render_elements(index_buffer_handle, indices_offset, nr_indices);
			return;
		}
	}

	static inline void add_to_bounding_box(Point* bbox, const Point& p) {
		if(p.x < bbox[0].x)
			bbox[0].x = p.x;
		if(p.y < bbox[0].y)
			bbox[0].y = p.y;

		if(p.x > bbox[1].x)
			bbox[1].x = p.x;
		if(p.y > bbox[1].y)
			bbox[1].y = p.y;
	}

	void Context::calculate_bounding_box(Point* bbox) {
		Point transformed[4];
		for(int k = 0; k < 4; k++) {
			transformed[k] = matrix[conversion_matrix].map_point(bbox[k]);
		}

		int k = 0;
		if(bounding_box_was_reset) {
			k = 1;
			bounding_box_was_reset = false;
			bounding_box[0] = transformed[0];
			bounding_box[1] = transformed[0];
		}

		for(; k < 4; k++) {
			add_to_bounding_box(bounding_box, transformed[k]);
		}
	}

	void Context::transform_bounding_box(Point* bbox, VGfloat *sp_ep) {
		Point transformed[4];
		for(int k = 0; k < 4; k++) {
			transformed[k] = matrix[conversion_matrix].map_point(bbox[k]);
		}

		Point bounding_box[2];

		bounding_box[0] = transformed[0];
		bounding_box[1] = transformed[0];

		for(auto k = 1; k < 4; k++) {
			add_to_bounding_box(bounding_box, transformed[k]);
		}

		sp_ep[0] = bounding_box[0].x;
		sp_ep[1] = bounding_box[0].y;
		sp_ep[2] = bounding_box[1].x;
		sp_ep[3] = bounding_box[1].y;
	}

	void Context::get_pixelsize(VGint& w, VGint& h) {
		w = current_framebuffer->width;
		h = current_framebuffer->height;
	}

	void Context::switch_mask_to(gnuVGFrameBuffer to_this_temporary) {
		auto temporary = mask;
		switch(to_this_temporary) {
		case Context::GNUVG_TEMPORARY_A:
			mask = temporary_a;
			temporary_a = temporary;
			break;
		case Context::GNUVG_TEMPORARY_B:
			mask = temporary_b;
			temporary_b = temporary;
			break;
		default:
			GNUVG_ERROR("Context::switch_mask_to() not called"
				    " with temporary buffer as parameter.\n");
			break;
		}
	}

	bool Context::create_framebuffer(const GraphicState &state,
					 FrameBuffer* destination,
					 VGImageFormat format,
					 VGint w, VGint h,
					 VGbitfield allowedQuality) {
		destination->width = w;
		destination->height = h;
		destination->framebuffer = eka2l1::drivers::create_bitmap(state.driver,
			eka2l1::vec2(w, h), 32);

		return (destination->framebuffer != 0);
	}

	void Context::delete_framebuffer(const GraphicState &state, FrameBuffer* framebuffer) {
		if(framebuffer == current_framebuffer) {
			render_to_framebuffer(state, &screen_buffer);
		}
		cmd_builder_.destroy_bitmap(framebuffer->framebuffer);
	}

	void Context::render_to_framebuffer(const GraphicState &state, const FrameBuffer* framebuffer) {
		current_framebuffer = framebuffer == nullptr ? (&screen_buffer) : framebuffer;

		cmd_builder_.bind_bitmap(current_framebuffer->framebuffer);
		render_scissors(state);

		if(current_framebuffer->width ==
		   buffer_width &&
		   current_framebuffer->height ==
		   buffer_height)
			return; // we can stop here - all is same from here

		buffer_width = current_framebuffer->width;
		buffer_height = current_framebuffer->height;

		cmd_builder_.set_viewport(eka2l1::rect(eka2l1::vec2(0, 0), eka2l1::vec2(buffer_width, buffer_height)));
		matrix_resize(buffer_width, buffer_height);
	}

	auto Context::get_internal_framebuffer(gnuVGFrameBuffer selection) -> const FrameBuffer* {
		switch(selection) {
		case Context::GNUVG_CURRENT_FRAMEBUFFER:
			return current_framebuffer;
		case Context::GNUVG_TEMPORARY_A:
			return &temporary_a;
		case Context::GNUVG_TEMPORARY_B:
			return &temporary_b;
		case Context::GNUVG_MASK_BUFFER:
			return &mask;
		}
		GNUVG_ERROR("Context::get_internal_framebuffer() returning nullptr\n");
		return nullptr;
	}

	Context::FrameBuffer* Context::get_temporary_framebuffer(const GraphicState &state,
							VGImageFormat format,
							VGint w, VGint h,
							VGbitfield allowedQuality) {
		for(auto k  = available_temporary_framebuffers.begin();
		         k != available_temporary_framebuffers.end();
		    k++) {
			auto f = (*k);
			if(f->width == w && f->height == h) {
				available_temporary_framebuffers.erase(k);
				return f;
			}
		}
		auto f = new FrameBuffer();
		if(!create_framebuffer(state, f, format, w, h, allowedQuality)) {
			delete f;
			f = nullptr;
		}
		return f;
	}

	void Context::return_temporary_framebuffer(FrameBuffer *fbf) {
		available_temporary_framebuffers.push_back(fbf);
	}

	void Context::copy_framebuffer_to_framebuffer(const GraphicState &state,
							  const FrameBuffer* dst,
						      const FrameBuffer* src,
						      VGint dx, VGint dy,
						      VGint sx, VGint sy,
						      VGint _width, VGint _height,
						      bool do_blend) {
		save_current_framebuffer();
		render_to_framebuffer(state, dst);

		auto caps = Shader::do_pattern;
		auto shader = state.shaderman->get_shader(state.driver, caps);

		if(do_blend)
			shader->set_blending(Shader::blend_src_over);
		else
			shader->set_blending(Shader::blend_src);

		shader->set_pattern_texture(src->framebuffer, eka2l1::drivers::addressing_option::clamp_to_edge);
		shader->set_pattern_size(_width, _height);

		float mtrx[] = {
			1.0, 0.0, 0.0, (float)(sx - dx),
			0.0, 1.0, 0.0, (float)(sy - dy),
			0.0, 0.0, 1.0,     0.0,
			0.0, 0.0, 0.0,     1.0
		};

		shader->set_pattern_matrix(mtrx);

		VGfloat x = (VGfloat)dx;
		VGfloat y = (VGfloat)dy;
		VGfloat width = (VGfloat)_width;
		VGfloat height = (VGfloat)_height;

		float vertices[] = {
			x,         y,
			x + width, y,
			x + width, y + height,
			x        , y + height
		};

		std::uint32_t indices[] = {
			0, 1, 2,
			0, 2, 3
		};

		setup_buffers_and_descriptors(state.driver, vertices, 8 * sizeof(float), 0,
			nullptr, 0, 0);

		std::size_t indices_offset = 0;
		eka2l1::drivers::handle index_buffer_handle = index_buffer_pusher.push_buffer(state.driver,
			reinterpret_cast<const std::uint8_t*>(indices), 6 * sizeof(std::uint32_t), indices_offset);

		shader->render_elements(index_buffer_handle, indices_offset, 6);
		restore_current_framebuffer(state);
	}

	void Context::copy_framebuffer_to_memory(const FrameBuffer* src,
						 void *memory, VGint stride,
						 VGImageFormat fmt,
						 VGint x, VGint y,
						 VGint width, VGint height) {
	}

	void Context::copy_memory_to_framebuffer(const FrameBuffer* dst,
						 const void *memory, VGint stride,
						 VGImageFormat fmt,
						 VGint x, VGint y,
						 VGint width, VGint height) {
		if(dst == &screen_buffer) {
			GNUVG_ERROR("Trying to write from memory directly to the screen buffer.\n");
			return; /* can't copy directly to framebuffer */
		}

		if(fmt != VG_sRGBA_8888) {
			GNUVG_ERROR("Only VG_sRGBA_8888 is supported currently.\n");
			return;
		}

		if(stride != 0) {
			GNUVG_ERROR("stride not supported for copying memory to and from an image.\n");
			return;
		}

		cmd_builder_.update_bitmap(dst->framebuffer, reinterpret_cast<const char*>(memory),
			32 * width * height, eka2l1::vec2(0, 0), eka2l1::vec2(width, height));
	}

	void Context::save_current_framebuffer() {
		framebuffer_storage.push(current_framebuffer);
	}

	void Context::restore_current_framebuffer(const GraphicState &state) {
		if(framebuffer_storage.size()) {
			render_to_framebuffer(state, framebuffer_storage.top());
			framebuffer_storage.pop();
		}
	}

	void Context::dereference(const GraphicState &state, VGHandle handle) {
		auto o = objects.find(handle);
		if(o != objects.end()) {
			o->second->free_resources(state);

			objects.erase(o);
			idalloc.free_id(handle);
		} else {
			set_error(VG_BAD_HANDLE_ERROR);
		}
	}
	
	std::shared_ptr<Object> Context::get_by_handle(VGHandle handle) {
		std::shared_ptr<Object> retval;

		auto o = objects.find(handle);
		if(o != objects.end())
			retval = (*o).second;
		else
			set_error(VG_BAD_HANDLE_ERROR);

		return retval;
	}
}

using namespace gnuVG;

namespace eka2l1 {

	/*********************
	 *
	 *  Getters and Setters
	 *
	 *********************/
	BRIDGE_FUNC_LIBRARY(VGErrorCode, vg_get_error_emu) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return VG_NO_CONTEXT_ERROR;
		}
		return ctx->get_error();
	}

	BRIDGE_FUNC_LIBRARY(void, vg_setf_emu, VGParamType type, VGfloat value) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgSetf((VGint)type, value);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_seti_emu, VGParamType type, VGint value) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgSeti(state, (VGint)type, value);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_setfv_emu, VGParamType type, VGint count, const VGfloat * values) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgSetfv((VGint)type, count, values);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_setiv_emu, VGParamType type, VGint count, const VGint * values) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys, &state);
		if (!ctx) {
			return;
		}
		ctx->vgSetiv(state, (VGint)type, count, values);
	}

	BRIDGE_FUNC_LIBRARY(VGfloat, vg_getf_emu, VGParamType type) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return 0.0f;
		}
		return ctx->vgGetf((VGint)type);
	}

	BRIDGE_FUNC_LIBRARY(VGint, vg_geti_emu, VGParamType type) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return -1;
		}
		return ctx->vgGeti((VGint)type);
	}

	BRIDGE_FUNC_LIBRARY(VGint, vg_get_vector_size_emu, VGParamType type) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return -1;
		}
		return ctx->vgGetVectorSize((VGint)type);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_getfv_emu, VGParamType type, VGint count, VGfloat * values) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgGetfv((VGint)type, count, values);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_getiv_emu, VGParamType type, VGint count, VGint * values) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgGetiv((VGint)type, count, values);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_set_parameterf_emu, VGHandle object, VGint paramType, VGfloat value) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		auto o = ctx->get<Object>(object);
		if(object)
			o->vgSetParameterf(paramType, value);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_set_parameteri_emu, VGHandle object, VGint paramType, VGint value) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		auto o = ctx->get<Object>(object);
		if(object)
			o->vgSetParameteri(paramType, value);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_set_parameterfv_emu, VGHandle object, VGint paramType,
		VGint count, const VGfloat * values) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		auto o = ctx->get<Object>(object);
		if(object)
			o->vgSetParameterfv(paramType, count, values);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_set_parameteriv_emu, VGHandle object, VGint paramType,
		VGint count, const VGint * values) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys, &state);
		if (!ctx) {
			return;
		}
		auto o = ctx->get<Object>(object);
		if(object)
			o->vgSetParameteriv(paramType, count, values);
	}

	BRIDGE_FUNC_LIBRARY(VGfloat, vg_get_parameterf_emu, VGHandle object, VGint paramType) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys, &state);
		if (!ctx) {
			return 0.0f;
		}
		auto o = ctx->get<Object>(object);
		if(object)
			return o->vgGetParameterf(paramType);
		return 0;
	}

	BRIDGE_FUNC_LIBRARY(VGint, vg_get_parameteri_emu, VGHandle object, VGint paramType) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys, &state);
		if (!ctx) {
			return -1;
		}
		auto o = ctx->get<Object>(object);
		if(object)
			return o->vgGetParameteri(paramType);
		return 0;
	}

	BRIDGE_FUNC_LIBRARY(VGint, vg_get_paremeter_vector_size_emu, VGHandle object, VGint paramType) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys, &state);
		if (!ctx) {
			return -1;
		}
		auto o = ctx->get<Object>(object);
		if(object)
			return o->vgGetParameterVectorSize(paramType);
		return 0;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_get_parameterfv_emu, VGHandle object, VGint paramType,
		VGint count, VGfloat * values) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys, &state);
		if (!ctx) {
			return;
		}
		auto o = ctx->get<Object>(object);
		if(object)
			o->vgGetParameterfv(paramType, count, values);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_get_parameteriv_emu, VGHandle object, VGint paramType,
		VGint count, VGint * values) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys, &state);
		if (!ctx) {
			return;
		}
		auto o = ctx->get<Object>(object);
		if(object)
			o->vgGetParameteriv(paramType, count, values);
	}

	/**********************
	 *
	 * Matrix Manipulation
	 *
	 **********************/
	BRIDGE_FUNC_LIBRARY(void, vg_load_identity_emu) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgLoadIdentity();
	}

	BRIDGE_FUNC_LIBRARY(void, vg_load_matrix_emu, const VGfloat * m) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgLoadMatrix(m);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_get_matrix_emu, VGfloat * m) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgGetMatrix(m);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_mult_matrix_emu, const VGfloat * m) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgMultMatrix(m);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_translate_emu, VGfloat tx, VGfloat ty) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgTranslate(tx, ty);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_scale_emu, VGfloat sx, VGfloat sy) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgScale(sx, sy);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_shear_emu, VGfloat shx, VGfloat shy) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgShear(shx, shy);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_rotate_emu, VGfloat angle) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return;
		}
		ctx->vgRotate(angle);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_clear_emu, VGint x, VGint y, VGint width, VGint height) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys, &state);
		if (!ctx) {
			return;
		}
		ctx->clear(state, x, y, width, height);
	}

	// TODO!
	/*
	const VGubyte * vgGetString(VGStringID name) {
		static VGubyte *vendor = (VGubyte *)strdup("Anton Persson");
		static VGubyte *renderer = (VGubyte *)strdup("gnuVG (modified for EKA2L1)");
		static VGubyte *version = (VGubyte *)strdup("1.1");
		static VGubyte *extensions = (VGubyte *)strdup("");

		switch(name) {
		case VG_VENDOR:
			return vendor;
		case VG_RENDERER:
			return renderer;
		case VG_VERSION:
			return version;
		case VG_EXTENSIONS:
			return extensions;
		case VG_STRING_ID_FORCE_SIZE:
			break;
		}

		return NULL;
	}*/
}
