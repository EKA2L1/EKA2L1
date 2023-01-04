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

	class FailedToCreateFontCacheException {};

	FontCache::FontCache(Context *context, const GraphicState &state, int w, int h) 
		: SkylineBinPack(w, h, true)
		, context(context) {
		if(!context->create_framebuffer(state, &framebuffer, VG_sRGBA_8888, w, h, VG_IMAGE_QUALITY_BETTER))
			throw FailedToCreateFontCacheException();

		context->save_current_framebuffer();
		context->render_to_framebuffer(state, &framebuffer);
		context->trivial_fill_area(state, 0, 0, w, h, 0.0, 0.0, 0.0, 0.0);
		context->restore_current_framebuffer(state);

		float w_f = (float)w;
		float h_f= (float)h;
		tex_mtrx[0] = 1.0 / w_f; tex_mtrx[3] = 0.0      ; tex_mtrx[6] = 0.0;
		tex_mtrx[1] = 0.0;       tex_mtrx[4] = 1.0 / h_f; tex_mtrx[7] = 0.0;
		tex_mtrx[2] = 0.0;       tex_mtrx[5] = 0.0;       tex_mtrx[8] = 1.0;
	}

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

	static inline void p_mul(VGfloat pp[], VGfloat mtrx[], VGfloat p[]) {
		pp[0] = mtrx[0] * p[0] + mtrx[3] * p[1] + mtrx[6] * p[2];
		pp[1] = mtrx[1] * p[0] + mtrx[4] * p[1] + mtrx[7] * p[2];
		pp[2] = mtrx[2] * p[0] + mtrx[5] * p[1] + mtrx[8] * p[2];
	}

	int Font::get_fc_scale() {
		VGfloat mtrx[9];
		context->vgGetMatrix(mtrx);

		mtrx[6] = mtrx[7] = 0.0; // skip translation

		VGfloat p_1[] = {0.0, 0.0, 1.0};
		VGfloat p_2[] = {0.0, 1.0, 1.0};
		VGfloat pp2[] = {0.0, 0.0, 0.0};
		VGfloat pp1[] = {0.0, 0.0, 0.0};
		p_mul(pp1, mtrx, p_1);
		p_mul(pp2, mtrx, p_2);

		VGfloat pp[] = {
			pp2[0] - pp1[0],
			pp2[1] - pp1[1],
			pp2[2] - pp1[2]
		};
		VGfloat l = sqrtf(pp[0] * pp[0] + pp[1] * pp[1]);

		l = 1.0 / l;
		return (int)l;
	}

	FontCache* Font::get_font_cache(const GraphicState &state, int fc_scale) {
		auto fc_p = font_caches.find(fc_scale);
		if(fc_p != font_caches.end()) {
			return (*fc_p).second;
		}

		GNUVG_DEBUG("Created font cache for fc_scale %d\n", fc_scale);
		auto fc = new FontCache(context, state, 512, 512);
		font_caches[fc_scale] = fc;
		return fc;
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
		if(glyph->path != VG_INVALID_HANDLE) {
			glyph->path->vgDrawPath(state, paintModes);
		}
		context->adjust_glyph_origin(glyph->escapement);
	}

	static VGfloat cord_cache_buffer[] = {0.0, 0.0};
	static std::vector<VGfloat> vrtc_cache_buffer; // vertices
	static std::vector<VGfloat> txtc_cache_buffer; // texture coordinates
	static std::vector<VGuint>  indc_cache_buffer; // indices

	static void reset_cache_buffer() {
		cord_cache_buffer[0] = 0.0;
		cord_cache_buffer[1] = 0.0;
		vrtc_cache_buffer.clear();
		txtc_cache_buffer.clear();
		indc_cache_buffer.clear();
	}

	inline static void push_coords(std::vector<float> &a, float x, float y) {
		a.push_back(x); a.push_back(y);
	}

	static void push_to_cache_buffer(float fc_scale, const FontCache::Rect &data) {
		int i = vrtc_cache_buffer.size() / 2;

		const auto x = cord_cache_buffer[0] - fc_scale * data.offset_x;
		const auto y = cord_cache_buffer[1] - fc_scale * data.offset_y;
		const auto w = fc_scale * data.width;
		const auto h = fc_scale * data.height;
		const auto w_t = data.width;
		const auto h_t = data.height;
		const auto x_t = data.x;
		const auto y_t = data.y;

		if(data.rotated) {
			// push vertice coords
			push_coords(vrtc_cache_buffer, x    , y);
			push_coords(vrtc_cache_buffer, x    , y + w);
			push_coords(vrtc_cache_buffer, x + h, y + w);
			push_coords(vrtc_cache_buffer, x + h, y);

			// push texture coords
			push_coords(txtc_cache_buffer, x_t      , y_t + h_t);
			push_coords(txtc_cache_buffer, x_t + w_t, y_t + h_t);
			push_coords(txtc_cache_buffer, x_t + w_t, y_t);
			push_coords(txtc_cache_buffer, x_t      , y_t);
		} else {
			// push vertice coords
			push_coords(vrtc_cache_buffer, x    , y);
			push_coords(vrtc_cache_buffer, x    , y + h);
			push_coords(vrtc_cache_buffer, x + w, y + h);
			push_coords(vrtc_cache_buffer, x + w, y);

			// push texture coords
			push_coords(txtc_cache_buffer, x_t      , y_t);
			push_coords(txtc_cache_buffer, x_t      , y_t + h_t);
			push_coords(txtc_cache_buffer, x_t + w_t, y_t + h_t);
			push_coords(txtc_cache_buffer, x_t + w_t, y_t);
		}

		// push indices
		indc_cache_buffer.push_back(i);
		indc_cache_buffer.push_back(i + 1);
		indc_cache_buffer.push_back(i + 2);

		indc_cache_buffer.push_back(i);
		indc_cache_buffer.push_back(i + 2);
		indc_cache_buffer.push_back(i + 3);
	}

	static void render_cache_buffer(Context *ctx, const GraphicState &state, const FontCache *fc) {
		if(indc_cache_buffer.size() == 0) return;

		ctx->render_texture_alpha_triangle_array(state,
			&(fc->framebuffer),
			vrtc_cache_buffer.data(), 8, 0,
			txtc_cache_buffer.data(), 8, 0,
			indc_cache_buffer.data(), indc_cache_buffer.size(),
			fc->get_texture_matrix()
			);
	}

	void Font::vgDrawGlyphs(const GraphicState &state, VGint glyphCount,
				const VGuint *glyphIndices,
				const VGfloat *adjustments_x,
				const VGfloat *adjustments_y,
				VGbitfield paintModes,
				VGboolean allowAutoHinting) {
		if(!(paintModes & (VG_FILL_PATH | VG_STROKE_PATH)))
			return;

		auto fc_scale = get_fc_scale();
		auto fc = get_font_cache(state, fc_scale);

		context->select_conversion_matrix(Context::GNUVG_MATRIX_GLYPH_USER_TO_SURFACE);

		reset_cache_buffer();

		auto gly_p = glyphs.data();
		for(auto k = 0; k < glyphCount; ++k) {
			auto glyphIndex = glyphIndices[k];

			FontCache::Rect cached_result;
			if(glyphIndex < glyphs.size()) {
				auto glyph = gly_p[glyphIndex];

				VGfloat adjustment[] = {
					glyph->escapement[0],
					glyph->escapement[1]
				};
				if(adjustments_x) adjustment[0] += adjustments_x[k];
				if(adjustments_y) adjustment[1] += adjustments_y[k];

				if(fc->lookup(glyphIndex, cached_result)) {
					push_to_cache_buffer(fc_scale, cached_result);
					for(auto k = 0; k < 2; ++k) {
						cord_cache_buffer[k] += adjustment[k];
					}
				} else {
					context->use_glyph_origin_as_pre_translation(glyph->origin);

					if(glyph->path != VG_INVALID_HANDLE) {
						glyph->path->vgDrawPath(state, paintModes);
					}
				}


				context->adjust_glyph_origin(adjustment);
			}
		}

		render_cache_buffer(context, state, fc);
	}
};

using namespace gnuVG;

namespace eka2l1 {
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
