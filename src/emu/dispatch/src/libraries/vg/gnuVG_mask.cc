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

#include <algorithm>
#include <functional>

#include <dispatch/libraries/vg/gnuVG_mask.hh>
#include <dispatch/libraries/vg/gnuVG_path.hh>
#include <dispatch/libraries/vg/gnuVG_emuutils.hh>
#include <dispatch/dispatcher.h>
#include <system/epoc.h>

#define MASK_R_CHANNEL_VALUE 0.0f
#define MASK_G_CHANNEL_VALUE 0.0f
#define MASK_B_CHANNEL_VALUE 0.0f

namespace gnuVG {
	MaskLayer::MaskLayer(Context* ctx, const GraphicState &state, VGint width, VGint height)
		: Object(ctx) {
		if(ctx->create_framebuffer(state,
			   &framebuffer, VG_sRGBA_8888,
			   width, height,
			   VG_IMAGE_QUALITY_BETTER))
			throw FailedToCreateMaskLayerException();
	}

	void MaskLayer::free_resources(const GraphicState &state) {
		if(context) {
			context->delete_framebuffer(state, &framebuffer);
		}
	}

	MaskLayer::~MaskLayer() {
	}

	void MaskLayer::vgFillMaskLayer(const GraphicState &state, VGint x, VGint y, VGint width, VGint height, VGfloat value) {
		if(context) {
			context->save_current_framebuffer();
			context->render_to_framebuffer(state, &framebuffer);
			context->trivial_fill_area(state, x, y, width, height,
					       MASK_R_CHANNEL_VALUE,
					       MASK_G_CHANNEL_VALUE,
					       MASK_B_CHANNEL_VALUE,
					       value);
			context->restore_current_framebuffer(state);
		}
	}

	void MaskLayer::vgCopyMask(const GraphicState &state, VGint dx, VGint dy,
				   VGint sx, VGint sy,
				   VGint width, VGint height) {
		if(context) {
			auto fbuf = context->get_internal_framebuffer(Context::GNUVG_CURRENT_FRAMEBUFFER);
			context->copy_framebuffer_to_framebuffer(state,
				fbuf, &framebuffer,
				dx, dy, sx, sy, width, height);
		}
	}

	/* inherited virtual interface */
	void MaskLayer::vgSetParameterf(VGint paramType, VGfloat value) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void MaskLayer::vgSetParameteri(VGint paramType, VGint value) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void MaskLayer::vgSetParameterfv(VGint paramType, VGint count, const VGfloat *values) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void MaskLayer::vgSetParameteriv(VGint paramType, VGint count, const VGint *values) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	VGfloat MaskLayer::vgGetParameterf(VGint paramType) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0.0f;
	}

	VGint MaskLayer::vgGetParameteri(VGint paramType) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0;
	}

	VGint MaskLayer::vgGetParameterVectorSize(VGint paramType) {
		return 0;
	}

	void MaskLayer::vgGetParameterfv(VGint paramType, VGint count, VGfloat *values) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void MaskLayer::vgGetParameteriv(VGint paramType, VGint count, VGint *values) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

};

using namespace gnuVG;

namespace eka2l1::dispatch {
	BRIDGE_FUNC_LIBRARY(VGMaskLayer, vg_create_mask_layer_emu, VGint width, VGint height) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return VG_INVALID_HANDLE;
		}

		if(context) {
			try {
				auto mskl = context->create<MaskLayer>(state, width, height);
				return (VGMaskLayer)mskl->get_handle();
			} catch(MaskLayer::FailedToCreateMaskLayerException e) {
				GNUVG_ERROR("vgCreateMaskLayer() - Failed to prepare framebuffer.\n");
			}
		} else {
			GNUVG_ERROR("vgCreateMaskLayer() - No valid context available.\n");
		}

		return VG_INVALID_HANDLE;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_destroy_mask_layer_emu, VGMaskLayer mskl) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		context->dereference(state, mskl);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_fill_mask_layer_emu, VGMaskLayer mskl, VGint x, VGint y,
		VGint width, VGint height, VGfloat value) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto ml = context->get<MaskLayer>(mskl);
		if(ml)
			ml->vgFillMaskLayer(state, x, y, width, height, value);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_copy_mask_emu, VGMaskLayer mskl, VGint dx, VGint dy,
		VGint sx, VGint sy, VGint width, VGint height) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto ml = context->get<MaskLayer>(mskl);
		if(ml)
			ml->vgCopyMask(state, dx, dy, sx, sy, width, height);
	}

	static void render_direct_helper(Context* ctx, const GraphicState &state, std::function<void()> perform_operation) {
		const auto blend = ctx->vgGeti(VG_BLEND_MODE);
		const auto transform = ctx->vgGeti(VG_COLOR_TRANSFORM);
		const auto scissors = ctx->vgGeti(VG_SCISSORING);
		ctx->vgSeti(state, VG_BLEND_MODE, VG_BLEND_SRC);
		ctx->vgSeti(state, VG_COLOR_TRANSFORM, VG_FALSE);
		ctx->vgSeti(state, VG_SCISSORING, VG_FALSE);
		ctx->save_current_framebuffer();

		auto fb_mask = ctx->get_internal_framebuffer(Context::GNUVG_MASK_BUFFER);

		ctx->render_to_framebuffer(state, fb_mask);

		perform_operation();
		ctx->vgSeti(state, VG_BLEND_MODE, blend);
		ctx->vgSeti(state, VG_COLOR_TRANSFORM, transform);
		ctx->vgSeti(state, VG_SCISSORING, scissors);

		ctx->restore_current_framebuffer(state);
		ctx->apply_state_changes(state);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_mask_emu, VGHandle mask, VGMaskOperation operation, VGint x, VGint y,
		VGint width, VGint height) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if(!context) {
			GNUVG_ERROR("vgMask() called without proper gnuVG context.\n");
			return;
		}

		if (width <= 0 || height <= 0 || operation < VG_CLEAR_MASK || operation > VG_SUBTRACT_MASK) {
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			return;
		}
		if (!context->ensure_mask_buffers(state)) {
			return;
		}

		if(operation == VG_CLEAR_MASK || operation == VG_FILL_MASK) {
			VGfloat value = operation == VG_CLEAR_MASK ? 0.0f : 1.0f;
			render_direct_helper(context, state,
					     [context, state, x, y, width, height, value]() {
						     context->trivial_fill_area(state, x, y, width, height,
									    MASK_R_CHANNEL_VALUE,
									    MASK_G_CHANNEL_VALUE,
									    MASK_B_CHANNEL_VALUE,
									    value);
					     }
				);
		} else {
			GNUVG_ERROR("vgMask() only supports VG_CLEAR_MASK and VG_FILL_MASK.\n");
			context->set_error(VG_BAD_HANDLE_ERROR);
			return;
		}

        if (context->cmd_builder_.need_flush()) {
            context->flush_to_driver(sys->get_dispatcher()->get_egl_controller(), state.driver);
        }
	}

	BRIDGE_FUNC_LIBRARY(void, vg_render_to_mask_emu, VGPath path, VGbitfield paintModes,
		VGMaskOperation operation) {
		GraphicState state;
		auto *context = gnuVG::get_active_context(sys, &state);
		if (!context) {
			return;
		}
		context->render_to_mask(state, path, paintModes, operation);
		if (context->cmd_builder_.need_flush()) {
			context->flush_to_driver(sys->get_dispatcher()->get_egl_controller(), state.driver);
		}
	}
}

namespace gnuVG {
	void Context::render_to_mask(const GraphicState &state, VGPath path_handle, VGbitfield paint_modes, VGMaskOperation operation) {
		auto path = std::dynamic_pointer_cast<Path>(get_by_handle(path_handle));
		if (!path) {
			set_error(VG_BAD_HANDLE_ERROR);
			return;
		}
		if ((paint_modes & ~(VG_FILL_PATH | VG_STROKE_PATH))
			|| operation < VG_CLEAR_MASK || operation > VG_SUBTRACT_MASK) {
			set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			return;
		}
		if (!paint_modes) {
			return;
		}

		if (!ensure_mask_buffers(state)) {
			return;
		}
		apply_state_changes(state);
		save_current_framebuffer();
		if (operation == VG_CLEAR_MASK || operation == VG_FILL_MASK) {
			render_to_framebuffer(state, &mask);
			cmd_builder_.set_color_mask(0b1111);
			cmd_builder_.clear({ 0.0f, 0.0f, 0.0f, operation == VG_FILL_MASK ? 1.0f : 0.0f, 0.0f, 0.0f },
				eka2l1::drivers::draw_buffer_bit_color_buffer);
			restore_current_framebuffer(state);
			return;
		}

		const auto old_fill = fill_paint;
		const auto old_stroke = stroke_paint;
		const auto old_blend = blend_mode;
		const auto old_conversion = conversion_matrix;
		const bool old_masking = mask_is_active;
		const bool old_transform = do_color_transform;
		const VGfloat old_translation[] = { pre_translation[0], pre_translation[1] };
		auto coverage_paint = std::make_shared<Paint>(this);
		coverage_paint->color.a = 1.0f;
		fill_paint = stroke_paint = coverage_paint;
		mask_is_active = false;
		do_color_transform = false;
		select_conversion_matrix(GNUVG_MATRIX_PATH_USER_TO_SURFACE);
		reset_pre_translation();

		// OpenVG applies fill and stroke coverage to the mask in separate passes.
		for (const auto mode : { VG_FILL_PATH, VG_STROKE_PATH }) {
			if (!(paint_modes & mode)) {
				continue;
			}
			render_to_framebuffer(state, &temporary_a);
			cmd_builder_.set_color_mask(0b1111);
			cmd_builder_.clear({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, eka2l1::drivers::draw_buffer_bit_color_buffer);
			blend_mode = blend_src;
			pending_changes |= SCISSOR_RECTS_CHANGED | BLEND_MODE_CHANGED;
			apply_state_changes(state);
			path->vgDrawPath(state, mode);

			render_to_framebuffer(state, &mask);
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);
			cmd_builder_.set_feature(eka2l1::drivers::graphics_feature::blend, true);
			using factor = eka2l1::drivers::blend_factor;
			const auto source = operation == VG_SET_MASK || operation == VG_UNION_MASK ? factor::one : factor::zero;
			const auto destination = operation == VG_SET_MASK ? factor::zero
				: operation == VG_INTERSECT_MASK ? factor::frag_out_alpha : factor::one_minus_frag_out_alpha;
			cmd_builder_.blend_formula(eka2l1::drivers::blend_equation::add, eka2l1::drivers::blend_equation::add,
				source, destination, source, destination);

			auto *shader = state.shaderman->get_shader(state.driver, Shader::do_texture);
			shader->set_current_graphics_command_builder(cmd_builder_);
			shader->use_shader();
			const float identity[] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
			shader->set_matrix(identity);
			shader->set_texture(temporary_a.color_buffer);
			float vertices[] = { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 };
			float texcoords[] = { 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1 };
			setup_buffers_and_descriptors(state.driver, vertices, sizeof(vertices), 0, texcoords, sizeof(texcoords), 0);
			shader->render_triangles(0, 6);
		}

		fill_paint = old_fill;
		stroke_paint = old_stroke;
		blend_mode = old_blend;
		mask_is_active = old_masking;
		do_color_transform = old_transform;
		select_conversion_matrix(old_conversion);
		std::copy(old_translation, old_translation + 2, pre_translation);
		restore_current_framebuffer(state);
		pending_changes |= SCISSOR_OPTION_CHANGED | BLEND_MODE_CHANGED;
		apply_state_changes(state);
	}
}
