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

#include <dispatch/libraries/vg/gnuVG_font.hh>
#include <dispatch/libraries/vg/gnuVG_debug.hh>
#include <dispatch/libraries/vg/gnuVG_emuutils.hh>

namespace gnuVG {
	void Font::Glyph::set_path(std::shared_ptr<Path> _path,
				   VGboolean _isHinted,
				   const VGfloat *_origin, const VGfloat *_escapement) {
		clear();
		path = _path;
		isHinted = _isHinted;
		for(int k = 0; k < 2; ++k) {
			origin[k] = _origin[k];
			escapement[k] = _escapement[k];
		}
	}

	void Font::Glyph::set_image(std::shared_ptr<Image> _image,
				    const VGfloat _origin[2], const VGfloat _escapement[2]) {
		clear();
		image = _image;

		for(int k = 0; k < 2; ++k) {
			origin[k] = _origin[k];
			escapement[k] = _escapement[k];
		}
	}

	void Font::Glyph::clear() {
		path.reset();
		image.reset();
		origin[0] = 0.0f;
		origin[1] = 0.0f;
		escapement[0] = 0.0f;
		escapement[1] = 0.0f;
	}

	Font::Glyph::Glyph() {
		clear();
	}

	Font::Glyph::~Glyph() {
		clear();
	}

	void Font::secure_max_glyphIndex(VGuint glyphIndex) {
		if(glyphIndex >= glyphs.size()) {
			int max_k = glyphIndex - glyphs.size() + 1;
			for(int k = 0; k < max_k; ++k)
				glyphs.push_back(new Glyph());
		}
	}

	Font::Font(Context *context, VGint glyphCapacityHint)
		: Object(context) {}

	Font::~Font() {
		for(auto glyph : glyphs)
			delete glyph;
	}

	/* inherited virtual interface */
	void Font::vgSetParameterf(VGint paramType, VGfloat value) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Font::vgSetParameteri(VGint paramType, VGint value) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Font::vgSetParameterfv(VGint paramType, VGint count, const VGfloat *values) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Font::vgSetParameteriv(VGint paramType, VGint count, const VGint *values) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	VGfloat Font::vgGetParameterf(VGint paramType) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0.0f;
	}

	VGint Font::vgGetParameteri(VGint paramType) {
		if(paramType == VG_FONT_NUM_GLYPHS)
			return (VGint)glyphs.size();

		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0;
	}


	VGint Font::vgGetParameterVectorSize(VGint paramType) {
		if(paramType == VG_FONT_NUM_GLYPHS)
			return 1;
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0;
	}


	void Font::vgGetParameterfv(VGint paramType, VGint count, VGfloat *values) {
		if(count == 1)
			values[0] = vgGetParameterf(paramType);
		else
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Font::vgGetParameteriv(VGint paramType, VGint count, VGint *values) {
		if(count == 1)
			values[0] = vgGetParameteri(paramType);
		else
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}


	void Font::vgSetGlyphToPath(VGuint glyphIndex,
				    std::shared_ptr<Path> path,
				    VGboolean isHinted,
				    const VGfloat *glyphOrigin,
				    const VGfloat *escapement) {
		secure_max_glyphIndex(glyphIndex);
		glyphs[glyphIndex]->set_path(path, isHinted, glyphOrigin, escapement);
	}

	void Font::vgSetGlyphToImage(VGuint glyphIndex,
				     std::shared_ptr<Image> image,
				     const VGfloat *glyphOrigin,
				     const VGfloat *escapement) {
		secure_max_glyphIndex(glyphIndex);
		glyphs[glyphIndex]->set_image(image, glyphOrigin, escapement);
	}

	void Font::vgClearGlyph(VGuint glyphIndex) {
		if(glyphIndex >= glyphs.size())
			return;
		glyphs[glyphIndex]->clear();
	}

	void Font::vgDrawGlyph(const GraphicState &state, VGuint glyphIndex,
			       VGbitfield paintModes,
			       VGboolean allowAutoHinting) {
		if(!(paintModes & (VG_FILL_PATH | VG_STROKE_PATH)))
			return;
		if(glyphIndex >= glyphs.size())
			return;
	
		context->select_conversion_matrix(Context::GNUVG_MATRIX_GLYPH_USER_TO_SURFACE);

		auto glyph = glyphs.data()[glyphIndex];
		context->use_glyph_origin_as_pre_translation(glyph->origin);
		if(glyph->path != nullptr) {
			glyph->path->vgDrawPath(state, paintModes);
		} else if (glyph->image != nullptr) {
			glyph->image->vgDrawImage(state, true);
		}
		context->adjust_glyph_origin(glyph->escapement);
	}

	void Font::vgDrawGlyphs(const GraphicState &state, VGint glyphCount,
				const VGuint *glyphIndices,
				const VGfloat *adjustments_x,
				const VGfloat *adjustments_y,
				VGbitfield paintModes,
				VGboolean allowAutoHinting) {
		if(!(paintModes & (VG_FILL_PATH | VG_STROKE_PATH)))
			return;

		context->select_conversion_matrix(Context::GNUVG_MATRIX_GLYPH_USER_TO_SURFACE);

		auto gly_p = glyphs.data();
		for(auto k = 0; k < glyphCount; ++k) {
			auto glyphIndex = glyphIndices[k];

			if(glyphIndex < glyphs.size()) {
				auto glyph = gly_p[glyphIndex];

				VGfloat adjustment[] = {
					glyph->escapement[0],
					glyph->escapement[1]
				};
				if(adjustments_x) adjustment[0] += adjustments_x[k];
				if(adjustments_y) adjustment[1] += adjustments_y[k];

				context->use_glyph_origin_as_pre_translation(glyph->origin);

				if(glyph->path != nullptr) {
					glyph->path->vgDrawPath(state, paintModes);
				} else if (glyph->image != nullptr) {
					glyph->image->vgDrawImage(state, true);
				}

				context->adjust_glyph_origin(adjustment);
			}
		}
	}
};

using namespace gnuVG;

namespace eka2l1::dispatch {
	/*********************
	 *
	 *    regular OpenVG API
	 *
	 *********************/

	BRIDGE_FUNC_LIBRARY(VGFont, vg_create_font_emu, VGint glyphCapacityHint) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return (VGFont)VG_INVALID_HANDLE;
		}

		auto font = context->create<Font>(glyphCapacityHint);

		if(font)
			return (VGFont)font->get_handle();

		return (VGFont)VG_INVALID_HANDLE;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_destroy_font_emu, VGFont font) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		context->dereference(state, font);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_set_glyph_to_path_emu, VGFont font, VGuint glyphIndex, VGPath path,
		VGboolean isHinted, const VGfloat *glyphOrigin, const VGfloat *escapement) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto f = context->get<Font>(font);
		auto p = context->get<Path>(path);
		if(f && p)
			f->vgSetGlyphToPath(glyphIndex, p, isHinted, glyphOrigin, escapement);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_set_glyph_to_image_emu, VGFont font, VGuint glyphIndex,
		VGImage image, const VGfloat *glyphOrigin, const VGfloat *escapement) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto f = context->get<Font>(font);
		auto i = context->get<Image>(image);
		if(f && i)
			f->vgSetGlyphToImage(glyphIndex, i, glyphOrigin, escapement);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_clear_glyph_emu, VGFont font, VGuint glyphIndex) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto f = context->get<Font>(font);
		if(f)
			f->vgClearGlyph(glyphIndex);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_draw_glyph_emu, VGFont font, VGuint glyphIndex, VGbitfield paintModes,
		VGboolean allowAutoHinting) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto f = context->get<Font>(font);
		if(f)
			f->vgDrawGlyph(state, glyphIndex, paintModes, allowAutoHinting);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_draw_glyphs_emu, VGFont font, VGint glyphCount, const VGuint *glyphIndices,
		const VGfloat *adjustments_x, const VGfloat *adjustments_y, VGbitfield paintModes,
		VGboolean allowAutoHinting) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto f = context->get<Font>(font);
		if(f)
			f->vgDrawGlyphs(state, glyphCount, glyphIndices, adjustments_x, adjustments_y,
				paintModes, allowAutoHinting);
	}
}
