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

using namespace gnuVG;

namespace eka2l1::dispatch {
	BRIDGE_FUNC_LIBRARY(void, vg_gaussian_blur_emu, VGImage dst, VGImage src, VGfloat stdDeviationX, VGfloat stdDeviationY, VGTilingMode tilingMode) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto d = context->get<Image>(dst);
		auto s = context->get<Image>(src);
		if(s) {
			auto sfb = s->get_framebuffer();

			// calculate matrix size
			stdDeviationX = stdDeviationX * 8.0 + 1.0;
			stdDeviationY = stdDeviationY * 8.0 + 1.0;

			// if d is found, render to d, otherwise
			// render to current target - this is a gnuVG extension
			if(d) {
				auto dfb = d->get_framebuffer();
				context->save_current_framebuffer();
				context->render_to_framebuffer(state, dfb);
			}
			context->trivial_render_framebuffer(
				state,
				sfb,
				(int)stdDeviationX,
				(int)stdDeviationY
				);
			if(d)
				context->restore_current_framebuffer(state);
		}
	}

}