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

#include <dispatch/libraries/vg/gnuVG_image.hh>
#include <dispatch/libraries/vg/gnuVG_emuutils.hh>

namespace gnuVG {
	Image::Image(Context* ctx, const GraphicState &state, VGImageFormat format,
		     VGint width, VGint height,
		     VGbitfield allowedQuality)
		: Object(ctx) {
		if(!ctx->create_framebuffer(state, &framebuffer, format, width, height, allowedQuality))
			throw FailedToCreateImageException();
	}

	void Image::free_resources(const GraphicState &state) {
		if (context) {
			context->delete_framebuffer(state, &framebuffer);
		}
	}

	Image::~Image() {
	}

	void Image::vgClearImage(const GraphicState &state, VGint x, VGint y, VGint width, VGint height) {
		if(context) {
			context->save_current_framebuffer();
			context->render_to_framebuffer(state, &framebuffer);
			context->clear(state, x, y, width, height);
			context->restore_current_framebuffer(state);
		}
	}

	void Image::vgImageSubData(const void * data, VGint dataStride,
				   VGImageFormat dataFormat,
				   VGint x, VGint y, VGint width, VGint height) {
		if(context) {
			context->copy_memory_to_framebuffer(
				&framebuffer,
				data, dataStride,
				dataFormat,
				x, y,
				width, height);
		}
	}

	void Image::vgGetImageSubData(void * data, VGint dataStride,
				      VGImageFormat dataFormat,
				      VGint x, VGint y,
				      VGint width, VGint height) {
		if (context) {
			context->copy_framebuffer_to_memory(
				&framebuffer,
				data, dataStride,
				dataFormat,
				x, y,
				width, height);
		}
	}

	Image* Image::vgChildImage(VGint x, VGint y, VGint width, VGint height) {
		return VG_INVALID_HANDLE;
	}

	Image* Image::vgGetParent() {
		return parent;
	}

	void Image::vgCopyImage(const GraphicState &state, 
				VGint dx, VGint dy,
				std::shared_ptr<Image> src, VGint sx, VGint sy,
				VGint width, VGint height,
				VGboolean dither) {
		if (context) {
			context->copy_framebuffer_to_framebuffer(
				state,
				&framebuffer, &src->framebuffer,
				dx, dy, sx, sy, width, height);
		}
	}

	void Image::vgDrawImage(const GraphicState &state) {
		if(context)
			context->trivial_render_framebuffer(state, &framebuffer);
	}

	void Image::vgSetPixels(const GraphicState &state, VGint dx, VGint dy,
				VGint sx, VGint sy,
				VGint width, VGint height) {
		if (context) {
			auto fbuf = context->get_internal_framebuffer(Context::GNUVG_CURRENT_FRAMEBUFFER);
			context->copy_framebuffer_to_framebuffer(state,
				fbuf, &framebuffer,
				dx, dy, sx, sy, width, height);
		}
	}

	void Image::vgGetPixels(const GraphicState &state, VGint dx, VGint dy,
				VGint sx, VGint sy,
				VGint width, VGint height) {
		if (context) {
			auto fbuf = context->get_internal_framebuffer(Context::GNUVG_CURRENT_FRAMEBUFFER);
			context->copy_framebuffer_to_framebuffer(state,
				&framebuffer, fbuf,
				dx, dy, sx, sy, width, height);
		}
	}

	/* inherited virtual interface */
	void Image::vgSetParameterf(VGint paramType, VGfloat value) {
	}

	void Image::vgSetParameteri(VGint paramType, VGint value) {
	}

	void Image::vgSetParameterfv(VGint paramType, VGint count, const VGfloat *values) {
	}

	void Image::vgSetParameteriv(VGint paramType, VGint count, const VGint *values) {
	}


	VGfloat Image::vgGetParameterf(VGint paramType) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0.0f;
	}

	VGint Image::vgGetParameteri(VGint paramType) {
		switch(paramType) {
		case VG_IMAGE_FORMAT:
			return VG_sRGBA_8888;
		case VG_IMAGE_WIDTH:
			return framebuffer.width;
		case VG_IMAGE_HEIGHT:
			return framebuffer.height;
		}
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return -1;
	}


	VGint Image::vgGetParameterVectorSize(VGint paramType) {
		return 1;
	}


	void Image::vgGetParameterfv(VGint paramType, VGint count, VGfloat *values) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Image::vgGetParameteriv(VGint paramType, VGint count, VGint *values) {
		if(count != 1)
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		switch(paramType) {
		case VG_IMAGE_FORMAT:
		case VG_IMAGE_WIDTH:
		case VG_IMAGE_HEIGHT:
			values[0] = vgGetParameteri(paramType);
			return;
		}
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

};

using namespace gnuVG;

namespace eka2l1 {
	BRIDGE_FUNC_LIBRARY(VGImage, vg_create_image_emu, VGImageFormat format, VGint width, VGint height,
		VGbitfield allowedQuality) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if(context) {
			try {
				auto image = context->create<Image>(state, format, width, height, allowedQuality);
				return (VGImage)image->get_handle();
			} catch(Image::FailedToCreateImageException e) {
				GNUVG_ERROR("vgCreateImage() - Failed to prepare framebuffer.\n");
			}
		} else {
			GNUVG_ERROR("vgCreateImage() - No valid context available.\n");
		}

		return VG_INVALID_HANDLE;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_destroy_image_emu, VGImage image) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		context->dereference(state, image);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_clear_image_emu, VGImage image, VGint x, VGint y, VGint width, VGint height) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto i = context->get<Image>(image);
		if(i)
			i->vgClearImage(state, x, y, width, height);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_image_subdata_emu, VGImage image, const void * data, VGint dataStride,
		VGImageFormat dataFormat, VGint x, VGint y, VGint width, VGint height) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto i = context->get<Image>(image);
		if(i)
			i->vgImageSubData(data, dataStride, dataFormat, x, y, width, height);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_get_image_subdata_emu, VGImage image, void * data, VGint dataStride,
		VGImageFormat dataFormat, VGint x, VGint y, VGint width, VGint height) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto i = context->get<Image>(image);
		if(i)
			i->vgGetImageSubData(data, dataStride, dataFormat, x, y, width, height);
	}

	BRIDGE_FUNC_LIBRARY(VGImage, vg_child_image_emu, VGImage parent, VGint x, VGint y, VGint width, VGint height) {
		return VG_INVALID_HANDLE;
	}

	BRIDGE_FUNC_LIBRARY(VGImage, vg_get_parent_emu, VGImage image) {
		return VG_INVALID_HANDLE;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_copy_image_emu, VGImage dst, VGint dx, VGint dy,
		VGImage src, VGint sx, VGint sy, VGint width, VGint height, VGboolean dither) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto d = context->get<Image>(dst);
		auto s = context->get<Image>(src);
		if(d && s)
			d->vgCopyImage(state, dx, dy, s, sx, sy, width, height, dither);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_draw_image_emu, VGImage image) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto i = context->get<Image>(image);
		if(i)
			i->vgDrawImage(state);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_set_pixels_emu, VGint dx, VGint dy, VGImage src, VGint sx, VGint sy,
		VGint width, VGint height) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto i = context->get<Image>(src);
		if(i)
			i->vgSetPixels(state, dx, dy, sx, sy, width, height);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_get_pixels_emu, VGImage dst, VGint dx, VGint dy,
		VGint sx, VGint sy, VGint width, VGint height) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto i = context->get<Image>(dst);
		if(i)
			i->vgGetPixels(state, dx, dy, sx, sy, width, height);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_write_pixels_emu, const void * data, VGint dataStride, VGImageFormat dataFormat,
		VGint dx, VGint dy, VGint width, VGint height) {
		Context *context = gnuVG::get_active_context(sys);

		if (context) {
			auto fbuf = context->get_internal_framebuffer(Context::GNUVG_CURRENT_FRAMEBUFFER);
			context->copy_memory_to_framebuffer(
				fbuf,
				data, dataStride,
				dataFormat,
				dx, dy,
				width, height);
		}
	}

	BRIDGE_FUNC_LIBRARY(void, vg_read_pixels_emu, void * data, VGint dataStride,
		VGImageFormat dataFormat, VGint sx, VGint sy, VGint width, VGint height) {
		Context *context = gnuVG::get_active_context(sys);

		if (context) {
			auto fbuf = context->get_internal_framebuffer(Context::GNUVG_CURRENT_FRAMEBUFFER);
			context->copy_framebuffer_to_memory(
				fbuf,
				data, dataStride,
				dataFormat,
				sx, sy,
				width, height);
		}
	}

	BRIDGE_FUNC_LIBRARY(void, vg_copy_pixels_emu, VGint dx, VGint dy, VGint sx, VGint sy, VGint width, VGint height) {
		GNUVG_ERROR("Unimplemented copy pixels!");
	}
}
