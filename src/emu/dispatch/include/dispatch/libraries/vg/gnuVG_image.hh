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

#ifndef __GNUVG_IMAGE_HH
#define __GNUVG_IMAGE_HH

#include <dispatch/libraries/vg/consts.h>

#include <dispatch/libraries/vg/gnuVG_context.hh>
#include <dispatch/libraries/vg/gnuVG_object.hh>
#include <common/linked.h>

namespace gnuVG {

	class Image : public Object {
	private:
		Image* parent = NULL;

		Context::FrameBuffer *framebuffer;
		Context::FrameBufferSubsetInfo subset_info;
		
		eka2l1::common::roundabout child_list;
		eka2l1::common::double_linked_queue_element child_link;

	public:
		class FailedToCreateImageException {};

		Context::FrameBuffer* get_framebuffer() {
			return framebuffer;
		}

		const Context::FrameBufferSubsetInfo &get_subset_info() const {
			return subset_info;
		}

		Image(Context* ctx, const GraphicState &state, VGImageFormat format,
		      VGint width, VGint height,
		      VGbitfield allowedQuality);

		Image(Context *ctx, Image *parent, int subx, int suby, int width, int height);

		virtual ~Image();

		/* OpenVG equivalent API - image manipulation */
		void vgClearImage(const GraphicState &state, VGint x, VGint y, VGint width, VGint height);
		void vgImageSubData(const GraphicState &state, const void * data, VGint dataStride,
				    VGImageFormat dataFormat,
				    VGint x, VGint y, VGint width, VGint height);
		void vgGetImageSubData(void * data, VGint dataStride,
				       VGImageFormat dataFormat,
				       VGint x, VGint y,
				       VGint width, VGint height);
		VGImage vgChildImage(VGint x, VGint y, VGint width, VGint height);
		Image* vgGetParent();
		void vgCopyImage(const GraphicState &state,
				 VGint dx, VGint dy,
				 std::shared_ptr<Image> src,
				 VGint sx, VGint sy,
				 VGint width, VGint height,
				 VGboolean dither);
		void vgDrawImage(const GraphicState &state, const bool for_glyph = false);
		void vgSetPixels(const GraphicState &state, VGint dx, VGint dy,
				 VGint sx, VGint sy,
				 VGint width, VGint height);
		void vgGetPixels(const GraphicState &state, VGint dx, VGint dy,
				 VGint sx, VGint sy,
				 VGint width, VGint height);

		/* inherited virtual interface */
		virtual void vgSetParameterf(VGint paramType, VGfloat value) override;
		virtual void vgSetParameteri(VGint paramType, VGint value) override;
		virtual void vgSetParameterfv(VGint paramType, VGint count, const VGfloat *values) override;
		virtual void vgSetParameteriv(VGint paramType, VGint count, const VGint *values) override;

		virtual VGfloat vgGetParameterf(VGint paramType) override;
		virtual VGint vgGetParameteri(VGint paramType) override;

		virtual VGint vgGetParameterVectorSize(VGint paramType) override;

		virtual void vgGetParameterfv(VGint paramType, VGint count, VGfloat *values) override;
		virtual void vgGetParameteriv(VGint paramType, VGint count, VGint *values) override;

		void free_resources(const GraphicState &state) override;
	};
}

#endif
