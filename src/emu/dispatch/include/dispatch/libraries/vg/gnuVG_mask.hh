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

#ifndef __GNUVG_MASK_HH
#define __GNUVG_MASK_HH

#include <dispatch/libraries/vg/consts.h>

#include <dispatch/libraries/vg/gnuVG_context.hh>
#include <dispatch/libraries/vg/gnuVG_object.hh>

namespace gnuVG {

	class MaskLayer : public Object {
	private:
		Context::FrameBuffer framebuffer;

	public:
		class FailedToCreateMaskLayerException {};

		const Context::FrameBuffer* get_framebuffer() {
			return &framebuffer;
		}

		MaskLayer(Context* ctx, const GraphicState &state, VGint width, VGint height);
		virtual ~MaskLayer();

		void free_resources(const GraphicState &state) override;

		/* OpenVG equivalent API - MaskLayer manipulation */
		void vgFillMaskLayer(const GraphicState &state, VGint x, VGint y, VGint width, VGint height, VGfloat value);
		void vgCopyMask(const GraphicState &state, VGint dx, VGint dy,
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

	};
}

#endif
