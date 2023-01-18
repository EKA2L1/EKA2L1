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

#include <dispatch/libraries/vg/gnuVG_emuutils.hh>
#include <dispatch/libraries/vg/gnuVG_path.hh>
#include <dispatch/libraries/vg/gnuVG_debug.hh>
#include <dispatch/dispatcher.h>
#include <string.h>

#include <system/epoc.h>

#define TESS_POLY_SIZE 3

namespace gnuVG {

	inline void Path::cleanup_path() {
		simplified.simplify_path(s_segments.data(),
					 s_coordinates.data(),
					 s_segments.size());
		// get top left, bottom right
		simplified.get_bounding_box(bounding_box);

		// calculate the top right, bottom left
		auto w = bounding_box[1].x - bounding_box[0].x;
		auto h = bounding_box[1].y - bounding_box[0].y;
		bounding_box[2].x = bounding_box[0].x + w;
		bounding_box[2].y = bounding_box[0].y;
		bounding_box[3].x = bounding_box[0].x;
		bounding_box[3].y = bounding_box[0].y + h;
		path_dirty = false;
	}

	void Path::vgSetParameterf(VGint paramType, VGfloat value) {
		switch(paramType) {
		case VG_PATH_FORMAT:
		case VG_PATH_DATATYPE:
		case VG_PATH_NUM_SEGMENTS:
		case VG_PATH_NUM_COORDS:
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			break;

		case VG_PATH_BIAS:
			bias = value;
			break;
		case VG_PATH_SCALE:
			scale = value;
			break;
		}
	}

	void Path::vgSetParameteri(VGint paramType, VGint value) {
		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Path::vgSetParameterfv(VGint paramType, VGint count, const VGfloat *values) {
		if(count != 1) {
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			return;
		}
		vgSetParameterf(paramType, values[0]);
	}

	void Path::vgSetParameteriv(VGint paramType, VGint count, const VGint *values) {
		if(count != 1) {
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
			return;
		}
		vgSetParameteri(paramType, values[0]);
	}

	VGfloat Path::vgGetParameterf(VGint paramType) {
		switch(paramType) {
		case VG_PATH_FORMAT:
		case VG_PATH_DATATYPE:
		case VG_PATH_NUM_SEGMENTS:
		case VG_PATH_NUM_COORDS:
			break;

		case VG_PATH_BIAS:
			return bias;

		case VG_PATH_SCALE:
			return scale;
		}

		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0.0f;
	}

	VGint Path::vgGetParameteri(VGint paramType) {
		switch(paramType) {
		case VG_PATH_FORMAT:
			return VG_PATH_FORMAT_STANDARD;
		case VG_PATH_DATATYPE:
			return dataType;
		case VG_PATH_NUM_SEGMENTS:
			return (VGint)s_segments.size();
		case VG_PATH_NUM_COORDS:
			return (VGint)s_coordinates.size();

		case VG_PATH_BIAS:
		case VG_PATH_SCALE:
			break;
		}

		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0.0f;
	}

	VGint Path::vgGetParameterVectorSize(VGint paramType) {
		switch(paramType) {
		case VG_PATH_FORMAT:
		case VG_PATH_DATATYPE:
		case VG_PATH_NUM_SEGMENTS:
		case VG_PATH_NUM_COORDS:
		case VG_PATH_BIAS:
		case VG_PATH_SCALE:
			return 1;
		}

		context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
		return 0;
	}

	void Path::vgGetParameterfv(VGint paramType, VGint count, VGfloat *values) {
		if(count == 1)
			values[0] = vgGetParameterf(paramType);
		else
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}

	void Path::vgGetParameteriv(VGint paramType, VGint count, VGint *values) {
		if(count == 1)
			values[0] = vgGetParameteri(paramType);
		else
			context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
	}


	Path::Path(Context *context, VGPathDatatype _dataType,
		   VGfloat _scale, VGfloat _bias,
		   VGbitfield _capabilities)
		: Object(context)
		, dataType(_dataType)
		, scale(_scale)
		, bias(_bias)
		, capabilities(_capabilities)
		, path_dirty(false)
		, simplified(context)
	{
	}


	Path::~Path() {
		vgClearPath(capabilities);

		if (tess)
            tessDeleteTess(tess);
    }

	void Path::vgClearPath(VGbitfield _capabilities) {
		path_dirty = true;
		s_segments.clear();
		s_coordinates.clear();
		s_segment_start_offset_in_coords.clear();
	}

	VGbitfield Path::vgGetPathCapabilities() {
		return capabilities;
	}


	void Path::vgAppendPath(std::shared_ptr<Path> srcPath) {
		path_dirty = true;
		/* XXX not implemented */
	}

	void Path::vgTransformPath(std::shared_ptr<Path> srcPath) {
		path_dirty = true;
		/* XXX not implemented */
	}

	VGboolean Path::vgInterpolatePath(std::shared_ptr<Path> startPath,
					  std::shared_ptr<Path> endPath, VGfloat amount) {
		path_dirty = true;
		/* XXX not implemented */
		return VG_FALSE;
	}

	VGfloat Path::vgPathLength(VGint startSegment, VGint numSegments) {
		/* XXX not implemented */
		return 0.0f;
	}

	void Path::vgPointAlongPath(VGint startSegment, VGint numSegments,
				    VGfloat distance,
				    VGfloat *x, VGfloat *y,
				    VGfloat *tangentX, VGfloat *tangentY) {
		/* XXX not implemented */
	}

	void Path::vgPathBounds(VGfloat * minX, VGfloat * minY,
				VGfloat * width, VGfloat * height) {
		if(path_dirty) cleanup_path();

		*minX	= bounding_box[0].x;
		*minY	= bounding_box[0].y;
		*width	= bounding_box[1].x - bounding_box[0].x;
		*height	= bounding_box[1].y - bounding_box[0].y;
	}

	void Path::vgPathTransformedBounds(VGfloat * minX, VGfloat * minY,
					   VGfloat * width, VGfloat * height) {
		if(path_dirty) cleanup_path();

		VGfloat sp_ep[4];

		context->transform_bounding_box(bounding_box, sp_ep);

		*minX	= sp_ep[0];
		*minY	= sp_ep[1];
		*width	= sp_ep[2] - sp_ep[0];
		*height	= sp_ep[3] - sp_ep[1];
	}

	static TESSalloc ma;
	static TESSalloc* ma_p = nullptr;

	void Path::vgDrawPath_reset_tesselator() {
		if(!ma_p) {
			ma_p = &ma;
			memset(ma_p, 0, sizeof(ma));
			ma.memalloc = GvgAllocator::gvg_alloc;
			ma.memfree = GvgAllocator::gvg_free;
			ma.memrealloc = GvgAllocator::gvg_realloc;

		}
		if(!tess)
			tess = tessNewTess(ma_p);
	}

	void Path::vgDrawPath_fill_regular(const GraphicState &state) {
		const float *vertices = NULL;
		const std::uint32_t *indices = NULL;
		std::int32_t nr_indices;
		std::int32_t nr_vertices;

		bool was_tesselated;

		{
			was_tesselated = tessTesselate(tess,
						       TESS_WINDING_ODD,
						       TESS_POLYGONS, TESS_POLY_SIZE, 2, 0) ? true : false;
		}

		GNUVG_DEBUG("vgDrawPath_fill_regular()\n");
		if(was_tesselated) {
			{
				vertices = (const float *) tessGetVertices(tess);
				indices = (const std::uint32_t *) tessGetElements(tess);
				nr_indices = (std::int32_t) tessGetElementCount(tess) * TESS_POLY_SIZE;

				// NOTE: Not really sure? We have to multiply with something
				nr_vertices = (std::int32_t) tessGetVertexCount(tess);
			}

			if(vertices != NULL && indices != NULL) {
//			GNUVG_DEBUG("    vgDrawPath: vertices(%p), indices(%p), nr indices(%d)\n", vertices, indices, nr_indices);
				context->use_pipeline(state, Context::GNUVG_SIMPLE_PIPELINE,
								     VG_FILL_PATH);
				context->setup_buffers_and_descriptors(state.driver, vertices, nr_vertices * 2 * sizeof(float),
					0, nullptr, 0, 0);

				context->render_elements(state, indices, nr_indices);
			}
		}
	}

	void Path::vgDrawPath(const GraphicState &state, VGbitfield paintModes) {
		if(path_dirty) cleanup_path();

		if(paintModes & VG_FILL_PATH) {
			{
				vgDrawPath_reset_tesselator();
				simplified.tesselate_fill_shape(tess);
			}
			vgDrawPath_fill_regular(state);
		}

		if(paintModes & VG_STROKE_PATH) {

			context->use_pipeline(state,
				Context::GNUVG_SIMPLE_PIPELINE,
				VG_STROKE_PATH);

			Context *context_copy = context;

			simplified.get_stroke_shape(
				[context_copy, state](const SimplifiedPath::StrokeData &stroke_data) {
					if(stroke_data.nr_vertices) {
						context_copy->setup_buffers_and_descriptors(state.driver,
							stroke_data.vertices,
							stroke_data.nr_vertices * 2 * sizeof(float),
							0, nullptr, 0, 0);
						context_copy->render_elements(
							state,
							stroke_data.indices,
							stroke_data.nr_indices);
					}
				}
				);
		}
		context->calculate_bounding_box(bounding_box);
	}
}

using namespace gnuVG;

namespace eka2l1::dispatch {
	BRIDGE_FUNC_LIBRARY(VGPath, vg_create_path_emu, VGint pathFormat, VGPathDatatype datatype,
		VGfloat scale, VGfloat bias, VGint segmentCapacityHint, VGint coordCapacityHint,
		VGbitfield capabilities) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return VG_INVALID_HANDLE;
		}

		auto path = context->create<Path>(datatype, scale, bias, capabilities);

		if(path)
			return (VGPath)path->get_handle();

		return VG_INVALID_HANDLE;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_destroy_path_emu, VGPath path) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		context->dereference(state, path);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_clear_path_emu, VGPath path, VGbitfield capabilities) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto p = context->get<Path>(path);
		if(p)
			p->vgClearPath(capabilities);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_remove_path_capabilities_emu, VGPath path, VGbitfield capabilities) {
	}

	BRIDGE_FUNC_LIBRARY(VGbitfield, vg_get_path_capabilities_emu, VGPath path) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return 0;
		}

		auto p = context->get<Path>(path);
		if(p)
			return p->vgGetPathCapabilities();

		return 0;
	}

	BRIDGE_FUNC_LIBRARY(void, vg_append_path_emu, VGPath dstPath, VGPath srcPath) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto sp = context->get<Path>(srcPath);
		auto dp = context->get<Path>(dstPath);

		if(sp && dp)
			dp->vgAppendPath(sp);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_append_path_data_emu, VGPath path, VGint numSegments,
		const VGubyte *pathSegments, const void *pathData) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto p = context->get<Path>(path);
		if(!p)
			return;

		switch(p->get_dataType()) {
		case VG_PATH_DATATYPE_FORCE_SIZE:
			break;
		case VG_PATH_DATATYPE_S_8:
			p->vgAppendPathData<int8_t>(numSegments, pathSegments, (const int8_t *)pathData);
			break;
		case VG_PATH_DATATYPE_S_16:
			p->vgAppendPathData<int16_t>(numSegments, pathSegments, (const int16_t *)pathData);
			break;
		case VG_PATH_DATATYPE_S_32:
			p->vgAppendPathData<int32_t>(numSegments, pathSegments, (const int32_t *)pathData);
			break;
		case VG_PATH_DATATYPE_F:
			p->vgAppendPathData<VGfloat>(numSegments, pathSegments, (const VGfloat *)pathData);
			break;
		}
	}

	BRIDGE_FUNC_LIBRARY(void, vg_modify_path_coords_emu, VGPath dstPath, VGint startIndex,
		VGint numSegments, const void * pathData) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto p = context->get<Path>(dstPath);
		if(!p)
			return;

		VGErrorCode error = VG_NO_ERROR;

		switch(p->get_dataType()) {
		case VG_PATH_DATATYPE_FORCE_SIZE:
			break;
		case VG_PATH_DATATYPE_S_8:
			error = p->vgModifyPathCoords<int8_t>(startIndex, numSegments, (const int8_t *)pathData);
			break;
		case VG_PATH_DATATYPE_S_16:
			error = p->vgModifyPathCoords<int16_t>(startIndex, numSegments, (const int16_t *)pathData);
			break;
		case VG_PATH_DATATYPE_S_32:
			error = p->vgModifyPathCoords<int32_t>(startIndex, numSegments, (const int32_t *)pathData);
			break;
		case VG_PATH_DATATYPE_F:
			error = p->vgModifyPathCoords<VGfloat>(startIndex, numSegments, (const VGfloat *)pathData);
			break;
		}

		if (error != VG_NO_ERROR) {
			context->set_error(error);
		}
	}

	BRIDGE_FUNC_LIBRARY(void, vg_transform_path_emu, VGPath dstPath, VGPath srcPath) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto sp = context->get<Path>(srcPath);
		auto dp = context->get<Path>(dstPath);
		if(dp && sp)
			dp->vgTransformPath(sp);
	}

	BRIDGE_FUNC_LIBRARY(VGboolean, vg_interpolate_path_emu, VGPath dstPath,
		VGPath startPath, VGPath endPath, VGfloat amount) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return VG_FALSE;
		}

		auto sp = context->get<Path>(startPath);
		auto dp = context->get<Path>(dstPath);
		auto ep = context->get<Path>(endPath);

		if(sp && ep && dp)
			return dp->vgInterpolatePath(sp, ep, amount);

		return VG_FALSE;
	}

	BRIDGE_FUNC_LIBRARY(VGfloat, vg_path_length_emu, VGPath path, VGint startSegment, VGint numSegments) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return 0.0f;
		}

		auto p = context->get<Path>(path);
		if(!p)
			return 0.0f;
		return p->vgPathLength(startSegment, numSegments);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_point_along_path_emu, VGPath path, VGint startSegment, VGint numSegments,
		VGfloat distance, VGfloat * x, VGfloat * y, VGfloat * tangentX, VGfloat * tangentY) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto p = context->get<Path>(path);
		if(!p)
			return;
		p->vgPointAlongPath(startSegment, numSegments, distance,
				    x, y, tangentX, tangentY);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_path_bounds_emu, VGPath path, VGfloat *minX, VGfloat *minY,
		VGfloat *width, VGfloat *height) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto p = context->get<Path>(path);
		if(!p)
			return;

		p->vgPathBounds(minX, minY, width, height);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_path_transformed_bounds_emu,
		VGPath path,
		VGfloat * minX, VGfloat * minY,
		VGfloat * width, VGfloat * height) {
		Context *context = gnuVG::get_active_context(sys);

		if (!context) {
			return;
		}

		auto p = context->get<Path>(path);
		if(!p)
			return;
		p->vgPathTransformedBounds(minX, minY, width, height);
	}

	BRIDGE_FUNC_LIBRARY(void, vg_draw_path_emu, VGPath path, VGbitfield paintModes) {
		GraphicState state;
		Context *context = gnuVG::get_active_context(sys, &state);

		if (!context) {
			return;
		}

		auto p = context->get<Path>(path);
		if(!p)
			return;
		if(!(paintModes & (VG_FILL_PATH | VG_STROKE_PATH)))
			return;

		context->select_conversion_matrix(Context::GNUVG_MATRIX_PATH_USER_TO_SURFACE);
		context->reset_pre_translation();
		context->apply_state_changes(state);

		p->vgDrawPath(state, paintModes);

        if (context->cmd_builder_.need_flush()) {
            context->flush_to_driver(sys->get_dispatcher()->get_egl_controller(), state.driver);
        }
	}

}
