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

#include <functional>

#include <dispatch/libraries/vg/gnuVG_mask.hh>
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

	static void render_helper(Context* ctx, const GraphicState &state, VGMaskOperation operation,
		std::function<void()> perform_operation) {
		// save current framebuffer
		ctx->save_current_framebuffer();

		auto fb_mask = ctx->get_internal_framebuffer(Context::GNUVG_MASK_BUFFER);

		// Render to mask
		ctx->render_to_framebuffer(state, fb_mask);

		// perform the operation
		perform_operation();

		// restore previously saved framebuffer
		ctx->restore_current_framebuffer(state);
	}

	static void render_direct_helper(Context* ctx, const GraphicState &state, std::function<void()> perform_operation) {
		// save current framebuffer
		ctx->save_current_framebuffer();

		auto fb_mask = ctx->get_internal_framebuffer(Context::GNUVG_MASK_BUFFER);

		// switch to temporary buffer A for rendering new mask data
		ctx->render_to_framebuffer(state, fb_mask);

		// perform the operation
		perform_operation();

		// restore previously saved framebuffer
		ctx->restore_current_framebuffer(state);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_mask_emu, VGHandle mask, VGMaskOperation operation, VGint x, VGint y,
		VGint width, VGint height) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if(!context) {
			GNUVG_ERROR("vgMask() called without proper gnuVG context.\n");
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

	// TODO: Move static global variable to cache somewhere somewhere
	/*
	BRIDGE_FUNC_LIBRARY(void, vg_render_to_mask_emu, VGPath path, VGbitfield paintModes,
		VGMaskOperation operation) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if(!context) {
			GNUVG_ERROR("vgRenderToMask() called without proper gnuVG context.\n");
			return;
		}

		if(path == VG_INVALID_HANDLE) {
			context->set_error(VG_BAD_HANDLE_ERROR);
			return;
		}

		render_helper(context,
			      operation,
			      [context, path, paintModes]() {
				      auto old_stroke = context->vgGetPaint(VG_STROKE_PATH);
				      auto old_fill = context->vgGetPaint(VG_FILL_PATH);

				      static VGPaint mpaint = VG_INVALID_HANDLE;
				      if(mpaint == VG_INVALID_HANDLE)
					      mpaint = vgCreatePaint();

				      if(mpaint) {
					      VGfloat rgba[] = {MASK_R_CHANNEL_VALUE,
								MASK_G_CHANNEL_VALUE,
								MASK_B_CHANNEL_VALUE,
								1.0f};

					      // remeber old settings for mask/scissors
					      VGint do_mask, do_scissors;
					      do_mask = vgGeti(VG_MASKING);
					      do_scissors = vgGeti(VG_SCISSORING);

					      // disable mask/scissors
					      vgSeti(VG_MASKING, VG_FALSE);
					      vgSeti(VG_SCISSORING, VG_FALSE);

					      vgSetParameterfv(mpaint, VG_PAINT_COLOR, 4, rgba);
					      vgSetParameteri(mpaint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);

					      vgSetPaint(mpaint, paintModes);

					      vgDrawPath(path, paintModes);

					      // restore mask/scissor settings
					      vgSeti(VG_MASKING, do_mask);
					      vgSeti(VG_SCISSORING, do_scissors);
				      }

				      vgSetPaint(old_stroke, VG_STROKE_PATH);
				      vgSetPaint(old_fill, VG_FILL_PATH);
			      }
			);
	}*/
}
