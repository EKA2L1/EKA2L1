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

#include <algorithm>
#include <cstring>
#include <limits>
#include <string.h>

#include <dispatch/libraries/vg/consts.h>
#include <dispatch/libraries/vg/gnuVG_context.hh>
#include <dispatch/libraries/vg/gnuVG_config.hh>
#include <dispatch/libraries/vg/gnuVG_image.hh>
#include <dispatch/libraries/vg/gnuVG_emuutils.hh>
#include <dispatch/libraries/egl/def.h>
#include <dispatch/libraries/egl/egl.h>

#include <system/epoc.h>
#include <dispatch/dispatcher.h>

namespace gnuVG {
	Context::Context()
		: user_matrix(GNUVG_MATRIX_PATH_USER_TO_SURFACE)
		, conversion_matrix(GNUVG_MATRIX_PATH_USER_TO_SURFACE)
		, stroke_width(1.0f)
		, stroke_dash_phase_reset(false)
		, miter_limit(4.0f)
		, cap_style(VG_CAP_BUTT)
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

	void Context::on_surface_changed(eka2l1::drivers::graphics_driver *driver, eka2l1::dispatch::egl_surface *prev_read, eka2l1::dispatch::egl_surface *prev_draw) {
		if (driver) {
			max_image_dimension = static_cast<VGint>(std::min<std::uint32_t>(driver->max_texture_size(), std::numeric_limits<VGint>::max()));
		}
		if (!draw_surface_) {
			return;
		}

		screen_buffer.framebuffer = draw_surface_->handle_;
		screen_buffer.color_buffer = draw_surface_->handle_;
		screen_buffer.scale_factor = draw_surface_->current_scale_;

		GraphicState temp;
		temp.driver = driver;

		resize(temp, draw_surface_->dimension_.x, draw_surface_->dimension_.y);
	}

	void Context::on_draw_surface_resized(eka2l1::drivers::graphics_driver *driver) {
		on_surface_changed(driver, draw_surface_, draw_surface_);
	}

	void Context::destroy(eka2l1::drivers::graphics_driver *driver, eka2l1::drivers::graphics_command_builder &builder) {
        vertex_buffer_pusher.destroy(builder);
        index_buffer_pusher.destroy(builder);

		for (gnuVG::Context::FrameBuffer *framebuffer: available_temporary_framebuffers) {
			delete_framebuffer_data_impl(framebuffer);
			delete framebuffer;
		}

		delete_framebuffer_data_impl(&mask);
		delete_framebuffer_data_impl(&temporary_a);
		delete_framebuffer_data_impl(&temporary_b);

		GraphicState temp_state;
		temp_state.driver = driver;
		temp_state.shaderman = nullptr;

		for (auto obj: objects) {
			obj.second->free_resources(temp_state);
		}

		for (auto obj: cleanup_lists) {
			obj->free_resources(temp_state);
		}

		while (!framebuffer_pool.empty()) {
			cmd_builder_.destroy(framebuffer_pool.front());
			framebuffer_pool.pop();
		}

		while (!texture_pool.empty()) {
			cmd_builder_.destroy(texture_pool.front());
			texture_pool.pop();
		}

        egl_context::destroy(driver, builder);
	}

	void Context::flush_to_driver(eka2l1::dispatch::egl_controller &controller, eka2l1::drivers::graphics_driver *driver, const bool is_frame_swap_flush) {
		GraphicState state;
		state.driver = driver;
		state.shaderman = &controller.get_vg_shaderman();

		flush_to_driver(state, is_frame_swap_flush);
	}

	void Context::flush_to_driver(const GraphicState &state, const bool is_frame_swap_flush) {
		eka2l1::drivers::graphics_command_builder transfer_builder;
        vertex_buffer_pusher.flush(transfer_builder);
        index_buffer_pusher.flush(transfer_builder);

        eka2l1::drivers::command_list retrieved = transfer_builder.retrieve_command_list();
        state.driver->submit_command_list(retrieved);

		// Reference is important!
		eka2l1::common::erase_elements(cleanup_lists, [&state](auto &cleanup) {
			if (cleanup.use_count() == 1) {
				cleanup->free_resources(state);
				return true;
			}
			return false;
		});

		retrieved = cmd_builder_.retrieve_command_list();

		state.driver->submit_command_list(retrieved);
		init_context_state();

		if (is_frame_swap_flush) {
			vertex_buffer_pusher.done_frame();
			index_buffer_pusher.done_frame();
        }
	}

	void Context::sync_blend_mode() {
		switch (blend_mode) {
		case blend_src: // blend_src equals no blending
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::blend, false);
			break;

		case blend_src_in:
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::blend, true);
			cmd_builder_.blend_formula(eka2l1::drivers::blend_equation::add,
				eka2l1::drivers::blend_equation::add,
				eka2l1::drivers::blend_factor::frag_out_alpha,
				eka2l1::drivers::blend_factor::one_minus_frag_out_alpha,
				eka2l1::drivers::blend_factor::current_alpha,
				eka2l1::drivers::blend_factor::zero);
			break;

		case blend_dst_over:
		case blend_dst_in:
		case blend_multiply:
		case blend_screen:
		case blend_darken:
		case blend_lighten:
		case blend_additive:
			GNUVG_ERROR("Unsupported blend mode {}", static_cast<int>(blend_mode));
			[[fallthrough]];

		case blend_src_over:
			// everything not supported
			// will default into src_over
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::blend, true);
			cmd_builder_.blend_formula(eka2l1::drivers::blend_equation::add,
				eka2l1::drivers::blend_equation::add,
				eka2l1::drivers::blend_factor::frag_out_alpha,
				eka2l1::drivers::blend_factor::one_minus_frag_out_alpha,
				eka2l1::drivers::blend_factor::frag_out_alpha,
				eka2l1::drivers::blend_factor::one_minus_frag_out_alpha);
			break;
		}
	}

	void Context::init_context_state() {
		if ((current_framebuffer == nullptr) || (current_framebuffer == &screen_buffer)) {
			cmd_builder_.bind_bitmap(draw_surface_->handle_);
		} else {
			cmd_builder_.bind_framebuffer(current_framebuffer->framebuffer, eka2l1::drivers::framebuffer_bind_read_draw);
		}

		cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::depth_test, false);
		cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::clipping, false);

		if (scissors_are_active && (nr_active_scissors > 0)) {
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, true);
		} else {
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);
		}

		sync_blend_mode();
	}

	void Context::apply_state_changes(const GraphicState &state) {
		if (pending_changes == 0) {
			return;
		}

		if (pending_changes & SCISSOR_RECTS_CHANGED) {
			render_scissors(state);
			pending_changes &= ~(SCISSOR_RECTS_CHANGED | SCISSOR_OPTION_CHANGED);
		}

		if (pending_changes & SCISSOR_OPTION_CHANGED) {
			if (scissors_are_active && (nr_active_scissors > 0)) {
				cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, true);
			} else {
				cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);
			}
		}

		if (pending_changes & BLEND_MODE_CHANGED) {
			sync_blend_mode();
		}

		pending_changes = 0;
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

	VGCapStyle Context::get_cap_style() {
		return cap_style;
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
			break;
		case VG_IMAGE_MODE:
			if (value == VG_DRAW_IMAGE_NORMAL || value == VG_DRAW_IMAGE_MULTIPLY || value == VG_DRAW_IMAGE_STENCIL)
				image_mode = static_cast<VGImageMode>(value);
			else
				set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			break;

		case VG_BLEND_MODE:
			switch((VGBlendMode)value) {
			case VG_BLEND_SRC:
				blend_mode = blend_src;
				break;
			case VG_BLEND_SRC_OVER:
				blend_mode = blend_src_over;
				break;
			case VG_BLEND_DST_OVER:
				blend_mode = blend_dst_over;
				break;
			case VG_BLEND_SRC_IN:
				blend_mode = blend_src_in;
				break;
			case VG_BLEND_DST_IN:
				blend_mode = blend_dst_in;
				break;
			case VG_BLEND_MULTIPLY:
				blend_mode = blend_multiply;
				break;
			case VG_BLEND_SCREEN:
				blend_mode = blend_screen;
				break;
			case VG_BLEND_DARKEN:
				blend_mode = blend_darken;
				break;
			case VG_BLEND_LIGHTEN:
				blend_mode = blend_lighten;
				break;
			case VG_BLEND_ADDITIVE:
				blend_mode = blend_additive;
				break;
			default:
				set_error(VG_ILLEGAL_ARGUMENT_ERROR);
				return;
			}

			pending_changes |= BLEND_MODE_CHANGED;
			break;

			/* Color Transformation */
		case VG_COLOR_TRANSFORM:
			do_color_transform = (((VGboolean)value) == VG_TRUE) ? true : false;
			break;

			/* Stroke parameters */
		case VG_STROKE_CAP_STYLE:
			switch((VGCapStyle)value) {
			case VG_CAP_BUTT:
			case VG_CAP_ROUND:
			case VG_CAP_SQUARE:
				cap_style = (VGCapStyle)value;
				break;
			default:
				set_error(VG_ILLEGAL_ARGUMENT_ERROR);
				break;
			}
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
			pending_changes |= SCISSOR_OPTION_CHANGED;
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
			pending_changes |= SCISSOR_RECTS_CHANGED;
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
			return static_cast<VGfloat>(vgGeti(paramType));

		case VG_MAX_GAUSSIAN_STD_DEVIATION:
			return GNUVG_MAX_GAUSSIAN_STD_DEVIATION;
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
			break;
		case VG_IMAGE_MODE:
			return image_mode;

		case VG_BLEND_MODE:
			switch(blend_mode) {
			case blend_src:
				return VG_BLEND_SRC;
			case blend_src_over:
				return VG_BLEND_SRC_OVER;
			case blend_dst_over:
				return VG_BLEND_DST_OVER;
			case blend_src_in:
				return VG_BLEND_SRC_IN;
			case blend_dst_in:
				return VG_BLEND_DST_IN;
			case blend_multiply:
				return VG_BLEND_MULTIPLY;
			case blend_screen:
				return VG_BLEND_SCREEN;
			case blend_darken:
				return VG_BLEND_DARKEN;
			case blend_lighten:
				return VG_BLEND_LIGHTEN;
			case blend_additive:
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
			return cap_style;
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

		case VG_MAX_IMAGE_WIDTH:
		case VG_MAX_IMAGE_HEIGHT:
			return max_image_dimension;

		case VG_MAX_IMAGE_PIXELS:
			return static_cast<VGint>(std::min<std::int64_t>(static_cast<std::int64_t>(max_image_dimension) * max_image_dimension,
				std::numeric_limits<VGint>::max() / 4));

		case VG_MAX_IMAGE_BYTES:
			return vgGeti(VG_MAX_IMAGE_PIXELS) * 4;

		case VG_MAX_KERNEL_SIZE:
			return GNUVG_MAX_KERNEL_SIZE;

		case VG_MAX_SEPARABLE_KERNEL_SIZE:
			return GNUVG_MAX_SEPARABLE_KERNEL_SIZE;

		case VG_MAX_GAUSSIAN_STD_DEVIATION:
			return GNUVG_MAX_GAUSSIAN_STD_DEVIATION;

		case VG_MAX_FLOAT:
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

	void Context::set_scissors_state(const bool enabled) {
		if (scissors_are_active && (nr_active_scissors > 0)) {
		} else {

		}
	}

	void Context::render_scissors(const GraphicState &state) {
		if(scissors_are_active &&  nr_active_scissors > 0) {
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::depth_test, false);
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
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::depth_test, false);
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
		eka2l1::drivers::input_descriptor descriptor[2]{};
		eka2l1::drivers::handle buffers[2];

		if (!vertex2d) {
			GNUVG_ERROR("No vertex buffer data to setup!");
			return;
		}

		if (!vertex_buffer_pusher.is_initialized()) {
			vertex_buffer_pusher.initialize(eka2l1::common::MB(4));
		}

		std::size_t offset = 0;
		buffers[0] = vertex_buffer_pusher.push_buffer(driver, reinterpret_cast<const std::uint8_t*>(vertex2d),
			vertex_buffer_size, offset);

		descriptor[0].buffer_slot = 0;
		descriptor[0].set_normalized(false);
		descriptor[0].set_format(2, eka2l1::drivers::data_format::sfloat);
		descriptor[0].offset = static_cast<int>(offset);
		descriptor[0].location = Shader::attrib_location_position;
		descriptor[0].stride = vertex_stride;

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
		descriptor[1].stride = texcoord_stride;
	
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
						 VGTilingMode tiling_mode,
						 const FrameBufferSubsetInfo *subset_info,
						 bool do_glyph, bool paint_alpha) {
		VGfloat w, h;

		if(subset_info && (subset_info->subset_width >= 0)) {
			w = (VGfloat)(subset_info->subset_width);
			h = (VGfloat)(subset_info->subset_height);
		} else {
			w = (VGfloat)(framebuffer->width);
			h = (VGfloat)(framebuffer->height);
		}

		VGfloat x = 0, y = 0;
		if (do_glyph) {
			x = pre_translation[0];
			y = pre_translation[1];
		}

		int matrix_select = (do_glyph ? GNUVG_MATRIX_GLYPH_USER_TO_SURFACE : GNUVG_MATRIX_IMAGE_USER_TO_SURFACE);
		paint_alpha = paint_alpha && matrix[matrix_select].c == 0.0f
			&& matrix[matrix_select].f == 0.0f && matrix[matrix_select].i == 1.0f;

		// calculate corners of image
		auto c1_p = matrix[matrix_select].map_point(Point(x, y));
		auto c2_p = matrix[matrix_select].map_point(Point(x, y + h));
		auto c3_p = matrix[matrix_select].map_point(Point(x + w, y + h));
		auto c4_p = matrix[matrix_select].map_point(Point(x + w, y));

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

		float left = 0.0f, bottom = 0.0f, right = 1.0f, top = 1.0f;
		if (paint_alpha && subset_info && subset_info->subset_x >= 0) {
			left = static_cast<float>(subset_info->subset_x) / framebuffer->width;
			bottom = static_cast<float>(subset_info->subset_y) / framebuffer->height;
			right = static_cast<float>(subset_info->subset_x + subset_info->subset_width) / framebuffer->width;
			top = static_cast<float>(subset_info->subset_y + subset_info->subset_height) / framebuffer->height;
		}
		float texture_coords[] = {
			left, bottom,
			left, top,
			right, top,
			right, bottom
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

		apply_state_changes(state);
		if (paint_alpha)
			select_conversion_matrix(static_cast<MatrixMode>(matrix_select));
		prepare_framebuffer_matrix(framebuffer, subset_info, do_glyph ? GNUVG_MATRIX_GLYPH_USER_TO_SURFACE : GNUVG_MATRIX_IMAGE_USER_TO_SURFACE);

		auto f = [this, vertices, vertices_full, mat, mat_full, indices, texture_coords, wrap_mode, state, do_glyph, paint_alpha]
			(const FrameBuffer *fbuffer,
			 bool do_horizontal_gauss,
			 int kern_diameter) {
			if (paint_alpha) {
				use_pipeline(state, GNUVG_SIMPLE_PIPELINE, VG_FILL_PATH,
					image_mode == VG_DRAW_IMAGE_STENCIL ? Shader::do_alpha_stencil : Shader::do_alpha_multiply);
				active_shader->set_matrix(mat);
				active_shader->set_texture(fbuffer->color_buffer);
				setup_buffers_and_descriptors(state.driver, vertices, 8 * sizeof(float), 0,
					texture_coords, 8 * sizeof(float), 0);
			} else {
				int caps = (do_glyph ? Shader::do_texture : Shader::do_pattern);
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
				active_shader->set_matrix(mat);

				if(do_color_transform)
					active_shader->set_color_transform(
						color_transform_scale,
						color_transform_bias);

				if (do_glyph) {
					active_shader->set_texture(fbuffer->color_buffer);

					setup_buffers_and_descriptors(state.driver, vertices, 8 * sizeof(float), 0,
						texture_coords, 8 * sizeof(float), 0);
				} else {
					active_shader->set_pattern_texture(fbuffer->color_buffer, wrap_mode);
					active_shader->set_pattern_matrix(do_horizontal_gauss ?
									mat_full :
									image_matrix_data);
					active_shader->set_pattern_size(fbuffer->width, fbuffer->height);

					setup_buffers_and_descriptors(state.driver, do_horizontal_gauss ? vertices_full : vertices,
						8 * sizeof(float), 0, nullptr, 0, 0);
				}
			}

			if (!index_buffer_pusher.is_initialized()) {
				index_buffer_pusher.initialize(eka2l1::common::MB(2));
			}

			std::size_t indices_offset = 0;
			eka2l1::drivers::handle index_buffer_handle = index_buffer_pusher.push_buffer(state.driver,
				reinterpret_cast<const std::uint8_t*>(indices), 6 * sizeof(std::uint32_t), indices_offset);

			active_shader->render_elements(index_buffer_handle, indices_offset, 6);
		};
		if((gaussian_width <= 1) && (gaussian_height <= 1))
			f(framebuffer, false, 1);
		else {
			auto tfbf = get_temporary_framebuffer(state, VG_sRGBA_8888,
							      framebuffer->width, framebuffer->height,
							      VG_IMAGE_QUALITY_BETTER);

			if(tfbf) {
				save_current_framebuffer();
				render_to_framebuffer(state, tfbf);
				auto keep_old_blend_mode = blend_mode;
				blend_mode = blend_src;
				pending_changes |= BLEND_MODE_CHANGED;
				trivial_fill_area(state, 0, 0,
						tfbf->width, tfbf->height,
						0.0, 0.0, 0.0, 0.0);
				blend_mode = keep_old_blend_mode;
				pending_changes |= BLEND_MODE_CHANGED;
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
		active_shader->set_current_graphics_command_builder(cmd_builder_);
		active_shader->use_shader();
		active_shader->set_matrix(mat);
		active_shader->set_color(col);
		if(do_color_transform)
			active_shader->set_color_transform(
				color_transform_scale,
				color_transform_bias);
		
		setup_buffers_and_descriptors(state.driver, vertices, vertices_count * sizeof(float), 0,
			nullptr, 0, 0);

		if (!index_buffer_pusher.is_initialized()) {
			index_buffer_pusher.initialize(eka2l1::common::MB(2));
		}

		std::size_t indices_offset = 0;
		eka2l1::drivers::handle index_buffer_handle = index_buffer_pusher.push_buffer(state.driver,
			reinterpret_cast<const std::uint8_t*>(indices), indices_count * sizeof(std::uint32_t), indices_offset);

		active_shader->render_elements(index_buffer_handle, indices_offset, indices_count);
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

		apply_state_changes(state);
		trivial_render_elements(state, vertices, 8, indices, 6, r, g, b, a);
	}

	void Context::prepare_framebuffer_matrix(const FrameBuffer* fbuf, const FrameBufferSubsetInfo *subset, MatrixMode matrix_mode) {
		Matrix image_to_clip;
		if (matrix_mode == GNUVG_MATRIX_FILL_PAINT_TO_USER || matrix_mode == GNUVG_MATRIX_STROKE_PAINT_TO_USER) {
			Matrix paint_to_surface;
			paint_to_surface.multiply(matrix[conversion_matrix], matrix[matrix_mode]);
			image_to_clip.multiply(screen_matrix, paint_to_surface);
		} else {
			image_to_clip.multiply(screen_matrix, matrix[matrix_mode]);
		}

		/* Create matrix to convert from final on-screen OpenGL
		 * coordinates back to texture space coordinates.
		 */
		Matrix temp_a, temp_b;
		if(subset && (subset->subset_x >= 0))
			temp_a.translate(-subset->subset_x, -subset->subset_y);
		temp_a.scale(fbuf->width, fbuf->height);
		temp_b.multiply(image_to_clip, temp_a);
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

	void Context::use_pipeline(const GraphicState &state, gnuVGPipeline new_pipeline, VGPaintMode _mode, int extra_shader_caps) {
		pipeline_mode = _mode;
		active_pipeline = new_pipeline;

		auto active_paint =
			pipeline_mode == VG_STROKE_PATH ? stroke_paint : fill_paint;
		int caps = Shader::do_flat_color | extra_shader_caps;
		if (!(extra_shader_caps & Shader::alpha_image_modes_mask))
			caps |= Shader::do_pretranslate;

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
		active_shader->set_matrix(conversion_matrix_data);
		if (caps & Shader::do_pretranslate)
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

		if(mask_is_active) active_shader->set_mask_texture(mask.color_buffer);

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
				auto subset = active_paint->pattern->get_subset_info();
				prepare_framebuffer_matrix(ptf, &subset, pipeline_mode == VG_FILL_PATH ? GNUVG_MATRIX_FILL_PAINT_TO_USER : GNUVG_MATRIX_STROKE_PAINT_TO_USER);

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

				active_shader->set_pattern_texture(ptf->color_buffer, wrap_mode);
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
			active_shader->set_current_graphics_command_builder(cmd_builder_);
			active_shader->render_triangles(first, count);
			return;
		}
	}

	void Context::render_elements(const GraphicState &state, const std::uint32_t *indices, std::int32_t nr_indices) {
		if(active_shader && indices && nr_indices > 0) {
			if (!index_buffer_pusher.is_initialized()) {
				index_buffer_pusher.initialize(eka2l1::common::MB(2));
			}

			std::size_t indices_offset = 0;
			eka2l1::drivers::handle index_buffer_handle = index_buffer_pusher.push_buffer(state.driver,
				reinterpret_cast<const std::uint8_t*>(indices),
				static_cast<std::size_t>(nr_indices) * sizeof(std::uint32_t), indices_offset);

			active_shader->set_current_graphics_command_builder(cmd_builder_);
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

	static std::size_t calculate_image_data_stride(VGImageFormat src_format, int width) {
		switch (src_format) {
		case VG_A_8:
			return width;

		case VG_sRGBA_5551:
		case VG_sRGB_565:
		case VG_sRGBA_4444:
			return width * 2;

		case VG_sRGBA_8888:
		case VG_sBGRA_8888:
		case VG_sABGR_8888:
		case VG_sARGB_8888:
		case VG_sXBGR_8888:
		case VG_sBGRX_8888:
		case VG_sXRGB_8888:
		case VG_sARGB_8888_PRE:
		case VG_sRGBA_8888_PRE:
			return width * 4;

		default:
			break;
		}

		GNUVG_ERROR("Unknown image format 0x{:X} to calculate data stride!", static_cast<int>(src_format));
		return 0;
	}

	// ARGB words are BGRA bytes in little-endian memory.
	static bool is_bgra_byte_order(VGImageFormat format) {
		return (format == VG_sARGB_8888) || (format == VG_sXRGB_8888) || (format == VG_sARGB_8888_PRE);
	}

	static bool is_premultiplied(VGImageFormat format) {
		return (format == VG_sRGBA_8888_PRE) || (format == VG_sARGB_8888_PRE);
	}

	static bool translate_vg_image_format_to_driver_format(VGImageFormat src_format, eka2l1::drivers::texture_format &in_format,
		eka2l1::drivers::texture_format &internal_format,
		eka2l1::drivers::texture_data_type &dtype,
		eka2l1::drivers::channel_swizzles &swizzle,
		bool strict) {
		swizzle = eka2l1::drivers::channel_swizzles{ eka2l1::drivers::channel_swizzle::red, eka2l1::drivers::channel_swizzle::green,
			eka2l1::drivers::channel_swizzle::blue, eka2l1::drivers::channel_swizzle::alpha };

		// For premultiplied, just treat the alpha as non-existen for now
		switch (src_format) {
		case VG_sRGB_565:
			in_format = eka2l1::drivers::texture_format::rgb;
			internal_format = strict ? eka2l1::drivers::texture_format::rgb : eka2l1::drivers::texture_format::rgba;
			dtype = eka2l1::drivers::texture_data_type::ushort_5_6_5;
			break;

		case VG_sRGBA_5551:
			in_format = eka2l1::drivers::texture_format::rgba;
			internal_format =  eka2l1::drivers::texture_format::rgba;
			dtype = eka2l1::drivers::texture_data_type::ushort_5_5_5_1;
			break;

		case VG_sRGBA_8888:
		case VG_sRGBA_8888_PRE:
			in_format = eka2l1::drivers::texture_format::rgba;
			internal_format = eka2l1::drivers::texture_format::rgba;
			dtype = eka2l1::drivers::texture_data_type::ubyte;
			break;
		
		case VG_sARGB_8888:
		case VG_sXRGB_8888:
		case VG_sARGB_8888_PRE:
			// Uploads swap these to RGBA bytes, so rendering into the texture stores the same layout as sampling expects.
			in_format = eka2l1::drivers::texture_format::rgba;
			internal_format = eka2l1::drivers::texture_format::rgba;
			dtype = eka2l1::drivers::texture_data_type::ubyte;
			swizzle[3] = (src_format == VG_sXRGB_8888) ? eka2l1::drivers::channel_swizzle::one : eka2l1::drivers::channel_swizzle::alpha;
			break;

		case VG_sBGRA_8888:
			in_format = eka2l1::drivers::texture_format::bgra;
			internal_format = eka2l1::drivers::texture_format::bgra;
			dtype = eka2l1::drivers::texture_data_type::ubyte;
			break;

		case VG_A_8:
			in_format = eka2l1::drivers::texture_format::r;
			internal_format = strict ? eka2l1::drivers::texture_format::r8 : eka2l1::drivers::texture_format::rgba;;
			dtype = eka2l1::drivers::texture_data_type::ubyte;
			swizzle = eka2l1::drivers::channel_swizzles{ eka2l1::drivers::channel_swizzle::one, eka2l1::drivers::channel_swizzle::one,
				eka2l1::drivers::channel_swizzle::one, eka2l1::drivers::channel_swizzle::red };
			break;

		default:
			return false;
        }

        return true;
    }

	bool Context::create_framebuffer(const GraphicState &state,
					 FrameBuffer* destination,
					 VGImageFormat format,
					 VGint w, VGint h,
					 VGbitfield allowedQuality,
					 bool non_scaling) {
        eka2l1::drivers::texture_format in_format;
        eka2l1::drivers::texture_format internal_format;
        eka2l1::drivers::texture_data_type dtype;
        eka2l1::drivers::channel_swizzles swizzle;

		if (!translate_vg_image_format_to_driver_format(format, in_format, internal_format, dtype,
			swizzle, state.driver->is_stricted())) {
			return false;
		}

		if (non_scaling) {
			destination->scale_factor = -1.0f;
		} else {
			destination->scale_factor = draw_surface_->current_scale_;
		}
		
        destination->width = w;
        destination->height = h;
        destination->format = format;

		if (!texture_pool.empty()) {
			destination->color_buffer = texture_pool.front();
			texture_pool.pop();

			cmd_builder_.recreate_texture(destination->color_buffer, 2, 0,
				internal_format, in_format, dtype, nullptr, 0,
				eka2l1::vec3(destination->width, destination->height, 0) * eka2l1::common::max<float>(1.0f, destination->scale_factor));
		} else {
			destination->color_buffer = eka2l1::drivers::create_texture(state.driver, 2, 0,
				internal_format, in_format, dtype, nullptr, 0,
				eka2l1::vec3(destination->width, destination->height, 0) * eka2l1::common::max<float>(1.0f, destination->scale_factor));
		}

        if (destination->color_buffer == 0) {
			return false;
		}

		cmd_builder_.set_swizzle(destination->color_buffer, swizzle[0], swizzle[1], swizzle[2], swizzle[3]);

		// Set some filter and addressing modes
		cmd_builder_.set_texture_addressing_mode(destination->color_buffer, eka2l1::drivers::addressing_direction::s, eka2l1::drivers::addressing_option::clamp_to_edge);
		cmd_builder_.set_texture_addressing_mode(destination->color_buffer, eka2l1::drivers::addressing_direction::t, eka2l1::drivers::addressing_option::clamp_to_edge);

		if (allowedQuality == VG_IMAGE_QUALITY_BETTER) {
			cmd_builder_.set_texture_filter(destination->color_buffer, false, eka2l1::drivers::filter_option::linear);
			cmd_builder_.set_texture_filter(destination->color_buffer, true, eka2l1::drivers::filter_option::linear);
		} else {
			cmd_builder_.set_texture_filter(destination->color_buffer, false, eka2l1::drivers::filter_option::nearest);
			cmd_builder_.set_texture_filter(destination->color_buffer, true, eka2l1::drivers::filter_option::nearest);
		}

		// Only create when needed, for now go for the texture
		destination->framebuffer = 0;
		destination->stencil_buffer = 0;

		return true;
	}

	void Context::delete_framebuffer_data_impl(const FrameBuffer *framebuffer) {
		if (framebuffer->framebuffer) {
			framebuffer_pool.push(framebuffer->framebuffer);
		}

		if (framebuffer->color_buffer) {
			texture_pool.push(framebuffer->color_buffer);
		}

		if (framebuffer->stencil_buffer) {
			texture_pool.push(framebuffer->stencil_buffer);
		}
	}

	void Context::delete_framebuffer(const GraphicState &state, FrameBuffer* framebuffer) {
		if(framebuffer == current_framebuffer) {
			render_to_framebuffer(state, &screen_buffer);
		}
		delete_framebuffer_data_impl(framebuffer);
	}

	bool Context::link_buffers_for_framebuffer_and_scale(const GraphicState &state,
		FrameBuffer *target) {
		if (target->color_buffer == 0) {
			return false;
		}

		if ((target->scale_factor > 0.0f) && (target->scale_factor != draw_surface_->current_scale_)) {	
			eka2l1::drivers::texture_format in_format;
			eka2l1::drivers::texture_format internal_format;
			eka2l1::drivers::texture_data_type dtype;
			eka2l1::drivers::channel_swizzles swizzle;

			if (!translate_vg_image_format_to_driver_format(target->format, in_format, internal_format, dtype,
				swizzle, state.driver->is_stricted())) {
				return false;
			}

			cmd_builder_.recreate_texture(target->color_buffer, 2, 0, in_format, internal_format, dtype, nullptr, 0,
				eka2l1::vec3(target->width, target->height, 0) * draw_surface_->current_scale_);

			target->scale_factor = draw_surface_->current_scale_;

			if (target->stencil_buffer) {
				cmd_builder_.recreate_texture(target->stencil_buffer, 2, 0,
					eka2l1::drivers::texture_format::depth24_stencil8,
					eka2l1::drivers::texture_format::depth_stencil,
					eka2l1::drivers::texture_data_type::uint_24_8, nullptr, 0,
					eka2l1::vec3(target->width, target->height, 0) * eka2l1::common::max<float>(1.0f, target->scale_factor));
			}
		}

		if (target->framebuffer == 0) {
			if (target->stencil_buffer == 0) {
				if (!texture_pool.empty()) {
					target->stencil_buffer = texture_pool.front();
					cmd_builder_.recreate_texture(target->stencil_buffer, 2, 0,
						eka2l1::drivers::texture_format::depth24_stencil8,
						eka2l1::drivers::texture_format::depth_stencil,
						eka2l1::drivers::texture_data_type::uint_24_8, nullptr, 0,
						eka2l1::vec3(target->width, target->height, 0) * eka2l1::common::max<float>(1.0f, target->scale_factor));

						texture_pool.pop();
					} else {
					target->stencil_buffer = eka2l1::drivers::create_texture(state.driver, 2, 0,
						eka2l1::drivers::texture_format::depth24_stencil8,
						eka2l1::drivers::texture_format::depth_stencil,
						eka2l1::drivers::texture_data_type::uint_24_8, nullptr, 0,
						eka2l1::vec3(target->width, target->height, 0) * eka2l1::common::max<float>(1.0f, target->scale_factor));
				}
			}

			if (target->stencil_buffer == 0) {
				return false;
			}

			int color_indicies[1] = { 0 };

			if (!framebuffer_pool.empty()) {
				target->framebuffer = framebuffer_pool.front();
				cmd_builder_.set_framebuffer_color_buffer(target->framebuffer, target->color_buffer, 0);
				cmd_builder_.set_framebuffer_depth_stencil_buffer(target->framebuffer, target->stencil_buffer, 0,
					target->stencil_buffer, 0);

				framebuffer_pool.pop();
			} else {
				target->framebuffer = eka2l1::drivers::create_framebuffer(state.driver, &target->color_buffer,
						color_indicies, 1, target->stencil_buffer, 0, target->stencil_buffer, 0);
			}
		}

		return (target->framebuffer != 0);
	}

	void Context::render_to_framebuffer(const GraphicState &state, FrameBuffer* framebuffer) {
		current_framebuffer = framebuffer == nullptr ? (&screen_buffer) : framebuffer;

		if ((framebuffer != &screen_buffer) && !link_buffers_for_framebuffer_and_scale(state, framebuffer)) {
			return;
		}

		if (framebuffer == &screen_buffer) {
			screen_buffer.scale_factor = draw_surface_->current_scale_;
			screen_buffer.width = draw_surface_->dimension_.x;
			screen_buffer.height = draw_surface_->dimension_.y;

			cmd_builder_.bind_bitmap(current_framebuffer->framebuffer);
		} else {
            cmd_builder_.bind_framebuffer(current_framebuffer->framebuffer, eka2l1::drivers::framebuffer_bind_read_draw);
        }

		if(current_framebuffer->width ==
		   buffer_width &&
		   current_framebuffer->height ==
		   buffer_height)
			return; // we can stop here - all is same from here

		buffer_width = current_framebuffer->width;
		buffer_height = current_framebuffer->height;

		eka2l1::rect viewport_transformed(eka2l1::vec2(0, 0), eka2l1::vec2(buffer_width, buffer_height));

		viewport_transformed.scale(draw_surface_->current_scale_);
		viewport_transformed.size.y *= -1;

		cmd_builder_.set_viewport(viewport_transformed);
		matrix_resize(buffer_width, buffer_height);
		
		render_scissors(state);
		pending_changes &= ~(SCISSOR_OPTION_CHANGED | SCISSOR_RECTS_CHANGED);
	}

	auto Context::get_internal_framebuffer(gnuVGFrameBuffer selection) -> FrameBuffer* {
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

	// The region a transfer may touch: the whole framebuffer, or a child image's subset of it.
	static void get_transfer_extent(const Context::FrameBuffer *fb, const Context::FrameBufferSubsetInfo *subset_info,
		std::int64_t &origin_x, std::int64_t &origin_y, std::int64_t &width, std::int64_t &height) {
		if (subset_info && (subset_info->subset_width >= 0)) {
			origin_x = subset_info->subset_x;
			origin_y = subset_info->subset_y;
			width = subset_info->subset_width;
			height = subset_info->subset_height;
		} else {
			origin_x = 0;
			origin_y = 0;
			width = fb->width;
			height = fb->height;
		}
	}

	// Clips a copy against both extents like computeBlitRegion() in the reference implementation, so source pixels
	// outside the source are skipped rather than clamped. Coordinates come back relative to the textures.
	static bool clip_blit_region(const Context::FrameBuffer *dst, const Context::FrameBufferSubsetInfo *dst_subset,
		const Context::FrameBuffer *src, const Context::FrameBufferSubsetInfo *src_subset,
		VGint &dx, VGint &dy, VGint &sx, VGint &sy, VGint &width, VGint &height) {
		std::int64_t dst_x, dst_y, dst_w, dst_h;
		std::int64_t src_x, src_y, src_w, src_h;
		get_transfer_extent(dst, dst_subset, dst_x, dst_y, dst_w, dst_h);
		get_transfer_extent(src, src_subset, src_x, src_y, src_w, src_h);

		const std::int64_t skip_x = std::max<std::int64_t>({ 0, -static_cast<std::int64_t>(sx), -static_cast<std::int64_t>(dx) });
		const std::int64_t skip_y = std::max<std::int64_t>({ 0, -static_cast<std::int64_t>(sy), -static_cast<std::int64_t>(dy) });
		const std::int64_t w = std::min<std::int64_t>({ width - skip_x, src_w - (sx + skip_x), dst_w - (dx + skip_x) });
		const std::int64_t h = std::min<std::int64_t>({ height - skip_y, src_h - (sy + skip_y), dst_h - (dy + skip_y) });

		if ((w <= 0) || (h <= 0)) {
			return false;
		}

		dx = static_cast<VGint>(dst_x + dx + skip_x);
		dy = static_cast<VGint>(dst_y + dy + skip_y);
		sx = static_cast<VGint>(src_x + sx + skip_x);
		sy = static_cast<VGint>(src_y + sy + skip_y);
		width = static_cast<VGint>(w);
		height = static_cast<VGint>(h);

		return true;
	}

	// Whether drawing sampled source colours straight into the destination texture would store the wrong values.
	static bool needs_format_conversion(VGImageFormat src_format, VGImageFormat dst_format) {
		return (dst_format == VG_A_8) || (is_premultiplied(src_format) != is_premultiplied(dst_format));
	}

	void Context::copy_framebuffer_to_framebuffer(const GraphicState &state,
							  FrameBuffer* dst,
						      FrameBuffer* src,
						      VGint dx, VGint dy,
						      VGint sx, VGint sy,
						      VGint _width, VGint _height,
						      const FrameBufferSubsetInfo *dst_subset,
						      const FrameBufferSubsetInfo *src_subset) {
		if (src == &screen_buffer) {
			GNUVG_ERROR("Copying from the drawing surface is not supported!\n");
			return;
		}

		if (!clip_blit_region(dst, dst_subset, src, src_subset, dx, dy, sx, sy, _width, _height)) {
			return;
		}

		if ((dst != &screen_buffer) && needs_format_conversion(src->format, dst->format)) {
			const VGint row_stride = static_cast<VGint>(calculate_image_data_stride(dst->format, _width));
			std::vector<std::uint8_t> pixels(static_cast<std::size_t>(row_stride) * _height);

			copy_framebuffer_to_memory(state, src, nullptr, pixels.data(), row_stride, dst->format, sx, sy, _width, _height);
			copy_memory_to_framebuffer(state, dst, nullptr, pixels.data(), row_stride, dst->format, dx, dy, _width, _height);
			return;
		}

		if (src->color_buffer == dst->color_buffer) {
			// Sampling the texture being rendered to is undefined, and vgCopyImage allows the regions to overlap.
			FrameBuffer staging;
			if (!create_framebuffer(state, &staging, src->format, _width, _height, VG_IMAGE_QUALITY_NONANTIALIASED, true)) {
				return;
			}

			copy_framebuffer_to_framebuffer(state, &staging, src, 0, 0, sx, sy, _width, _height);
			copy_framebuffer_to_framebuffer(state, dst, &staging, dx, dy, 0, 0, _width, _height);
			delete_framebuffer(state, &staging);
			return;
		}

		save_current_framebuffer();

		apply_state_changes(state);
		render_to_framebuffer(state, dst);

		// Scissoring applies to the drawing surface (vgSetPixels), never to copies into images.
		if (dst != &screen_buffer) {
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);
		}

		auto shader = state.shaderman->get_shader(state.driver, Shader::do_texture);
		shader->set_current_graphics_command_builder(cmd_builder_);
		shader->use_shader();

		float identity[] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};

		shader->set_matrix(identity);

		Blending blend_mode_prev = blend_mode;

		if (blend_mode != blend_src) {
			blend_mode = blend_src;
			sync_blend_mode();
		}

		cmd_builder_.set_texture_addressing_mode(src->color_buffer, eka2l1::drivers::addressing_direction::s, eka2l1::drivers::addressing_option::clamp_to_edge);
		cmd_builder_.set_texture_addressing_mode(src->color_buffer, eka2l1::drivers::addressing_direction::t, eka2l1::drivers::addressing_option::clamp_to_edge);
		shader->set_texture(src->color_buffer);

		auto c1 = screen_matrix.map_point(Point((VGfloat)dx, (VGfloat)dy));
		auto c2 = screen_matrix.map_point(Point((VGfloat)(dx + _width), (VGfloat)(dy + _height)));

		float vertices[] = {
			c1.x, c1.y,
			c2.x, c1.y,
			c2.x, c2.y,
			c1.x, c2.y
		};

		const float u1 = (float)sx / src->width;
		const float v1 = (float)sy / src->height;
		const float u2 = (float)(sx + _width) / src->width;
		const float v2 = (float)(sy + _height) / src->height;

		float texture_coords[] = {
			u1, v1,
			u2, v1,
			u2, v2,
			u1, v2
		};

		std::uint32_t indices[] = {
			0, 1, 2,
			0, 2, 3
		};

		setup_buffers_and_descriptors(state.driver, vertices, 8 * sizeof(float), 0,
			texture_coords, 8 * sizeof(float), 0);

		if (!index_buffer_pusher.is_initialized()) {
			index_buffer_pusher.initialize(eka2l1::common::MB(2));
		}

		std::size_t indices_offset = 0;
		eka2l1::drivers::handle index_buffer_handle = index_buffer_pusher.push_buffer(state.driver,
			reinterpret_cast<const std::uint8_t*>(indices), 6 * sizeof(std::uint32_t), indices_offset);

		shader->render_elements(index_buffer_handle, indices_offset, 6);

		if (blend_mode_prev != blend_mode) {
			blend_mode = blend_mode_prev;
			sync_blend_mode();

			pending_changes &= ~BLEND_MODE_CHANGED;
		}

		restore_current_framebuffer(state);
	}

	// Clips a transfer between client memory and an image to the image extent (a child image's subset when given), like
	// computeBlitRegion() in the reference implementation. Returns false when nothing is left to transfer.
	static bool clip_image_transfer(const Context::FrameBuffer *fb, const Context::FrameBufferSubsetInfo *subset_info,
		std::ptrdiff_t stride, std::ptrdiff_t pixel_size, VGint &x, VGint &y, VGint &width, VGint &height,
		std::ptrdiff_t &memory_offset) {
		std::int64_t origin_x, origin_y, extent_w, extent_h;
		get_transfer_extent(fb, subset_info, origin_x, origin_y, extent_w, extent_h);

		const std::int64_t x0 = std::max<std::int64_t>(x, 0);
		const std::int64_t y0 = std::max<std::int64_t>(y, 0);
		const std::int64_t x1 = std::min<std::int64_t>(static_cast<std::int64_t>(x) + width, extent_w);
		const std::int64_t y1 = std::min<std::int64_t>(static_cast<std::int64_t>(y) + height, extent_h);

		if ((x0 >= x1) || (y0 >= y1)) {
			return false;
		}

		memory_offset = static_cast<std::ptrdiff_t>(y0 - y) * stride + static_cast<std::ptrdiff_t>(x0 - x) * pixel_size;

		x = static_cast<VGint>(origin_x + x0);
		y = static_cast<VGint>(origin_y + y0);
		width = static_cast<VGint>(x1 - x0);
		height = static_cast<VGint>(y1 - y0);

		return true;
	}

	// Converts a 32-bit pixel (alpha in byte 3 for every supported layout) between straight and premultiplied alpha.
	static void convert_alpha_mode(std::uint8_t *pixel, bool to_premultiplied) {
		const std::uint32_t alpha = pixel[3];

		for (int channel = 0; channel < 3; channel++) {
			if (to_premultiplied) {
				pixel[channel] = static_cast<std::uint8_t>((pixel[channel] * alpha + 127) / 255);
			} else {
				pixel[channel] = static_cast<std::uint8_t>((alpha == 0) ? 0 : std::min<std::uint32_t>(255, (pixel[channel] * 255 + alpha / 2) / alpha));
			}
		}
	}

	static std::uint8_t swizzle_channel(const std::uint8_t *texel, eka2l1::drivers::channel_swizzle swizzle) {
		switch (swizzle) {
		case eka2l1::drivers::channel_swizzle::red:
			return texel[0];
		case eka2l1::drivers::channel_swizzle::green:
			return texel[1];
		case eka2l1::drivers::channel_swizzle::blue:
			return texel[2];
		case eka2l1::drivers::channel_swizzle::alpha:
			return texel[3];
		case eka2l1::drivers::channel_swizzle::zero:
			return 0;
		default:
			break;
		}

		return 0xFF;
	}

	// Packs one colour into client memory, the inverse of how copy_memory_to_framebuffer() uploads it.
	static void write_image_pixel(VGImageFormat fmt, const std::uint8_t *rgba, std::uint8_t *out) {
		switch (fmt) {
		case VG_sRGBA_8888:
		case VG_sRGBA_8888_PRE:
			std::memcpy(out, rgba, 4);
			break;

		case VG_sARGB_8888:
		case VG_sXRGB_8888:
		case VG_sARGB_8888_PRE:
		case VG_sBGRA_8888:
			out[0] = rgba[2];
			out[1] = rgba[1];
			out[2] = rgba[0];
			out[3] = (fmt == VG_sXRGB_8888) ? 0xFF : rgba[3];
			break;

		case VG_sRGB_565: {
			const std::uint16_t value = static_cast<std::uint16_t>(((rgba[0] >> 3) << 11) | ((rgba[1] >> 2) << 5) | (rgba[2] >> 3));
			std::memcpy(out, &value, sizeof(value));
			break;
		}

		case VG_sRGBA_5551: {
			const std::uint16_t value = static_cast<std::uint16_t>(((rgba[0] >> 3) << 11) | ((rgba[1] >> 3) << 6) | ((rgba[2] >> 3) << 1) | (rgba[3] >> 7));
			std::memcpy(out, &value, sizeof(value));
			break;
		}

		case VG_A_8:
			out[0] = rgba[3];
			break;

		default:
			break;
		}
	}

	void Context::copy_framebuffer_to_memory(const GraphicState &state,
						 FrameBuffer* src,
						 const FrameBufferSubsetInfo *subset_info,
						 void *memory, VGint stride,
						 VGImageFormat fmt,
						 VGint x, VGint y,
						 VGint width, VGint height) {
		if (!memory || (width <= 0) || (height <= 0)) {
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			return;
		}

		if ((src == &screen_buffer) || (src->scale_factor > 0.0f)) {
			GNUVG_ERROR("Reading back from the drawing surface is not supported!\n");
			return;
		}

		eka2l1::drivers::texture_format in_format;
		eka2l1::drivers::texture_format internal_format;
		eka2l1::drivers::texture_data_type dtype;
		eka2l1::drivers::channel_swizzles src_swizzle;
		eka2l1::drivers::channel_swizzles dst_swizzle;

		if (!translate_vg_image_format_to_driver_format(fmt, in_format, internal_format, dtype,
			dst_swizzle, state.driver->is_stricted())) {
			set_error(VG_UNSUPPORTED_IMAGE_FORMAT_ERROR);
			return;
		}

		if (!translate_vg_image_format_to_driver_format(src->format, in_format, internal_format, dtype,
			src_swizzle, state.driver->is_stricted())) {
			return;
		}

		const std::ptrdiff_t pixel_size = static_cast<std::ptrdiff_t>(calculate_image_data_stride(fmt, 1));
		const std::ptrdiff_t row_stride = stride;

		std::ptrdiff_t memory_offset = 0;
		if (!clip_image_transfer(src, subset_info, row_stride, pixel_size, x, y, width, height, memory_offset)) {
			return;
		}

		if (!link_buffers_for_framebuffer_and_scale(state, src)) {
			return;
		}

		// The read is synchronous, so everything queued so far must reach the driver first.
		flush_to_driver(state);

		std::vector<std::uint8_t> texels(static_cast<std::size_t>(width) * height * 4);
		eka2l1::drivers::read_framebuffer(state.driver, src->framebuffer, eka2l1::vec2(x, y), eka2l1::vec2(width, height),
			eka2l1::drivers::texture_format::rgba, eka2l1::drivers::texture_data_type::ubyte, texels.data());

		std::uint8_t *out_base = reinterpret_cast<std::uint8_t*>(memory) + memory_offset;
		const bool dst_premultiplied = is_premultiplied(fmt);
		const bool convert_alpha = (is_premultiplied(src->format) != dst_premultiplied);

		for (VGint row = 0; row < height; row++) {
			const std::uint8_t *texel = texels.data() + static_cast<std::size_t>(row) * width * 4;
			std::uint8_t *out = out_base + static_cast<std::ptrdiff_t>(row) * row_stride;

			for (VGint col = 0; col < width; col++, texel += 4, out += pixel_size) {
				// Texels are RGBA; the source swizzle fills in what sampling sees for alpha-only and X formats.
				std::uint8_t rgba[4] = {
					swizzle_channel(texel, src_swizzle[0]),
					swizzle_channel(texel, src_swizzle[1]),
					swizzle_channel(texel, src_swizzle[2]),
					swizzle_channel(texel, src_swizzle[3])
				};

				if (convert_alpha) {
					convert_alpha_mode(rgba, dst_premultiplied);
				}

				write_image_pixel(fmt, rgba, out);
			}
		}
	}

	void Context::copy_memory_to_framebuffer(const GraphicState &state,
						 FrameBuffer* dst,
						 const FrameBufferSubsetInfo *subset_info,
						 const void *memory, VGint stride,
						 VGImageFormat fmt,
						 VGint x, VGint y,
						 VGint width, VGint height) {
		if (!memory || (width <= 0) || (height <= 0)) {
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			return;
		}

		eka2l1::drivers::texture_format in_format;
		eka2l1::drivers::texture_format internal_format;
		eka2l1::drivers::texture_data_type dtype;
		eka2l1::drivers::channel_swizzles swizzle;

		if (!translate_vg_image_format_to_driver_format(fmt, in_format, internal_format, dtype,
			swizzle, state.driver->is_stricted())) {
			set_error(VG_UNSUPPORTED_IMAGE_FORMAT_ERROR);
			return;
		}

		if ((dst == &screen_buffer) || (dst->scale_factor > 0.0f)) {
			// The surface texture is not addressable in VG pixels, so stage the data and draw it like vgSetPixels.
			FrameBuffer staging;
			if (!create_framebuffer(state, &staging, fmt, width, height, VG_IMAGE_QUALITY_NONANTIALIASED, true)) {
				return;
			}

			copy_memory_to_framebuffer(state, &staging, nullptr, memory, stride, fmt, 0, 0, width, height);
			copy_framebuffer_to_framebuffer(state, dst, &staging, x, y, 0, 0, width, height);
			delete_framebuffer(state, &staging);
			return;
		}

		const std::ptrdiff_t pixel_size = static_cast<std::ptrdiff_t>(calculate_image_data_stride(fmt, 1));
		std::ptrdiff_t row_stride = stride;

		std::ptrdiff_t memory_offset = 0;
		if (!clip_image_transfer(dst, subset_info, row_stride, pixel_size, x, y, width, height, memory_offset)) {
			return;
		}

		const std::uint8_t *data = reinterpret_cast<const std::uint8_t*>(memory) + memory_offset;
		const std::size_t clipped_row_size = static_cast<std::size_t>(pixel_size * width);

		// Repack when the driver cannot take the rows as they are: ARGB needs its bytes swapped, alpha may need
		// converting, and a zero or negative stride (Qt passes flipped QImages this way) is no unpack row length.
		const bool dst_premultiplied = is_premultiplied(dst->format);
		const bool swap_red_blue = is_bgra_byte_order(fmt);
		const bool convert_alpha = (pixel_size == 4) && (is_premultiplied(fmt) != dst_premultiplied);
		std::vector<std::uint8_t> packed;

		if ((row_stride <= 0) || swap_red_blue || convert_alpha) {
			packed.resize(clipped_row_size * height);

			for (VGint row = 0; row < height; row++) {
				std::uint8_t *packed_row = packed.data() + row * clipped_row_size;
				std::memcpy(packed_row, data + static_cast<std::ptrdiff_t>(row) * row_stride, clipped_row_size);

				if (!swap_red_blue && !convert_alpha) {
					continue;
				}

				for (std::size_t offset = 0; offset < clipped_row_size; offset += 4) {
					if (swap_red_blue) {
						std::swap(packed_row[offset], packed_row[offset + 2]);
					}

					// The X byte is padding; the pixel is opaque.
					if (fmt == VG_sXRGB_8888) {
						packed_row[offset + 3] = 0xFF;
					}

					if (convert_alpha) {
						convert_alpha_mode(packed_row + offset, dst_premultiplied);
					}
				}
			}

			data = packed.data();
			row_stride = pixel_size * width;
		}

		if ((row_stride % pixel_size) != 0) {
			LOG_WARN(eka2l1::HLE_DISPATCHER, "Stride byte count is odd compare to pixel size! VG Image may contains distortion!");
		}

		const std::size_t total_data_size = static_cast<std::size_t>((height - 1) * row_stride) + clipped_row_size;

		cmd_builder_.update_texture(dst->color_buffer, reinterpret_cast<const char*>(data),
			total_data_size, 0, in_format, dtype, eka2l1::vec3(x, y, 0), eka2l1::vec3(width, height, 0),
			row_stride / pixel_size, 1);
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
			if (o->second.use_count() == 1) {
				// Free right away, only this instance is around
				o->second->free_resources(state);
			} else {
				// Queue for later destruction
				cleanup_lists.push_back(o->second);
			}
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

namespace eka2l1::dispatch {
    BRIDGE_FUNC_LIBRARY(void, vg_flush_emu) {
        gnuVG::Context *ctx = get_active_context(sys);
        if (ctx) {
            ctx->flush_to_driver(sys->get_dispatcher()->get_egl_controller(), sys->get_graphics_driver());
        }
    }

	/*********************
	 *
	 *  Getters and Setters
	 *
	 *********************/
	BRIDGE_FUNC_LIBRARY(VGint, vg_get_error_emu) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return VG_NO_CONTEXT_ERROR;
		}
		return ctx->get_error();
	}

	BRIDGE_FUNC_LIBRARY(void, vg_finish_emu) {
		vg_flush_emu(sys);
		egl_wait_gl_emu(sys);
	}

	BRIDGE_FUNC_LIBRARY(VGint, vg_hardware_query_emu, VGHardwareQueryType key, VGint setting) {
		gnuVG::Context *ctx = get_active_context(sys);
		if (!ctx) {
			return VG_HARDWARE_UNACCELERATED;
		}
		bool valid = false;
		if (key == VG_IMAGE_FORMAT_QUERY) {
			valid = !(setting & ~0xFF) && ((setting & 0x3F) <= VG_A_4);
		} else if (key == VG_PATH_DATATYPE_QUERY) {
			valid = (setting >= VG_PATH_DATATYPE_S_8) && (setting <= VG_PATH_DATATYPE_F);
		}
		if (!valid) {
			ctx->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			return VG_HARDWARE_UNACCELERATED;
		}
		return VG_HARDWARE_ACCELERATED;
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
		gnuVG::Context *ctx = get_active_context(sys, &state);
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

        if (ctx->cmd_builder_.need_flush()) {
            ctx->flush_to_driver(sys->get_dispatcher()->get_egl_controller(), state.driver);
        }
	}

    BRIDGE_FUNC_LIBRARY(address, vg_get_string_emu, std::uint32_t pname) {
		GraphicState state;
		gnuVG::Context *ctx = get_active_context(sys, &state);

        if (!ctx) {
            return 0;
        }

        if ((pname != VG_EXTENSIONS_EMU) && (pname != VG_VENDOR_EMU) && (pname != VG_RENDERER_EMU) && (pname != VG_VERSION_EMU)) {
            ctx->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
            return 0;
        }

        return sys->get_dispatcher()->retrieve_static_string(pname);
    }
}
