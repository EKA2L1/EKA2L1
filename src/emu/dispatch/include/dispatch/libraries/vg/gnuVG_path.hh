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

#ifndef __GNUVG_PATH_HH
#define __GNUVG_PATH_HH

#include <dispatch/libraries/vg/gnuVG_context.hh>
#include <dispatch/libraries/vg/gnuVG_object.hh>
#include <dispatch/libraries/vg/gnuVG_simplified_path.hh>
#include <dispatch/libraries/vg/gnuVG_memclasses.hh>

#include <tesselator.h>
#include <vector>
#include <string>

namespace gnuVG {
	class Path : public Object {
	public:
		std::vector<VGubyte> s_segments;
		std::vector<VGfloat> s_coordinates;
		std::vector<std::size_t> s_segment_start_offset_in_coords;

	protected:
		VGPathDatatype dataType;
		VGfloat scale, bias;
		VGbitfield capabilities;

		bool path_dirty;
		SimplifiedPath simplified;

	private:
		/* simplified bounding box - {top left, bottom right, top right, bottom left} */
		Point bounding_box[4];

		TESStesselator* tess = nullptr;

		void vgDrawPath_reset_tesselator();
		void vgDrawPath_tesselate_subpath();

		void vgDrawPath_fill_regular(const GraphicState &state); // regular tesselation
		void vgDrawPath_stroke();

		void cleanup_path();

	public:
		VGPathDatatype get_dataType() {
			return dataType;
		}

		Path(Context *context, VGPathDatatype dataType,
		     VGfloat scale, VGfloat bias,
		     VGbitfield capabilities);

		virtual ~Path();

		/* OpenVG equivalent API */
		virtual void vgSetParameterf(VGint paramType, VGfloat value);
		virtual void vgSetParameteri(VGint paramType, VGint value);
		virtual void vgSetParameterfv(VGint paramType, VGint count, const VGfloat *values);
		virtual void vgSetParameteriv(VGint paramType, VGint count, const VGint *values);

		virtual VGfloat vgGetParameterf(VGint paramType);
		virtual VGint vgGetParameteri(VGint paramType);

		virtual VGint vgGetParameterVectorSize(VGint paramType);

		virtual void vgGetParameterfv(VGint paramType, VGint count, VGfloat *values);
		virtual void vgGetParameteriv(VGint paramType, VGint count, VGint *values);

		void vgClearPath(VGbitfield capabilities);
		void vgRemovePathCapabilities(VGbitfield capabilities);
		VGbitfield vgGetPathCapabilities();

		void vgAppendPath(std::shared_ptr<Path> srcPath);

		template<typename T>
		void vgAppendPathData(VGint numSegments,
				      const VGubyte *pathSegments,
				      const T *pathData) {
			path_dirty = true;

			VGint remaining_segments = numSegments;
			const VGubyte *sgmt = pathSegments;
			const T *dat = pathData;

			while(remaining_segments) {
				s_segments.push_back((*sgmt));
				s_segment_start_offset_in_coords.push_back(s_coordinates.size());

				switch( (*sgmt) & (~0x00000001) ) {
					/* commands with ZERO parameters */
				case VG_CLOSE_PATH:
					break;

					/* commands with ONE parameter */
				case VG_HLINE_TO:
				case VG_VLINE_TO:
					s_coordinates.push_back((VGfloat)(*(dat++)));
					break;

					/* commands with TWO parameters */
				case VG_SQUAD_TO:
				case VG_LINE_TO:
				case VG_MOVE_TO:
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					break;

					/* commands with FOUR parameters */
				case VG_SCUBIC_TO:
				case VG_QUAD_TO:
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					break;

					/* commands with FIVE parameters */
				case VG_SCCWARC_TO:
				case VG_SCWARC_TO:
				case VG_LCCWARC_TO:
				case VG_LCWARC_TO:
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					break;

					/* commands with SIX parameters */
				case VG_CUBIC_TO:
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					s_coordinates.push_back((VGfloat)(*(dat++)));
					break;
				}
				sgmt++;
				remaining_segments--;
			}
		}

		void vgAppendPathData(VGint numSegments,
				      const VGubyte *pathSegments,
				      const VGfloat *pathData) {
			path_dirty = true;

			VGint remaining_segments = numSegments;
			const VGubyte *sgmt = pathSegments;
			size_t nrcoords = 0;

			while(remaining_segments) {
				switch( (*sgmt) & (~0x00000001) ) {
					/* commands with ZERO parameters */
				case VG_CLOSE_PATH:
					break;

					/* commands with ONE parameter */
				case VG_HLINE_TO:
				case VG_VLINE_TO:
					nrcoords += 1;
					break;

					/* commands with TWO parameters */
				case VG_SQUAD_TO:
				case VG_LINE_TO:
				case VG_MOVE_TO:
					nrcoords += 2;
					break;

					/* commands with FOUR parameters */
				case VG_SCUBIC_TO:
				case VG_QUAD_TO:
					nrcoords += 4;
					break;

					/* commands with FIVE parameters */
				case VG_SCCWARC_TO:
				case VG_SCWARC_TO:
				case VG_LCCWARC_TO:
				case VG_LCWARC_TO:
					nrcoords += 5;
					break;

					/* commands with SIX parameters */
				case VG_CUBIC_TO:
					nrcoords += 6;
					break;
				}
				sgmt++;
				remaining_segments--;
			}
			s_segments.insert(s_segments.end(), pathSegments, pathSegments + numSegments);
			s_coordinates.insert(s_coordinates.end(), pathData, pathData + nrcoords);
		}

		template <typename T>
		VGErrorCode vgModifyPathCoords(VGint startIndex, VGint numSegments, const T *pathData) {
			if (numSegments == 0) {
				return VG_NO_ERROR;
			}

			if ((startIndex < 0) || (startIndex + numSegments > s_segments.size())) {
				return VG_ILLEGAL_ARGUMENT_ERROR;
			}

			path_dirty = true;
			size_t nrcoords = 0;

			for (VGint i = 0; i < numSegments; i++) {
				std::size_t num_coords = 0;
				std::size_t start_offset = s_segment_start_offset_in_coords[startIndex + i];
				if ((startIndex + i + 1) >= s_segments.size()) {
					num_coords = s_coordinates.size() - start_offset;
				} else {
					num_coords = s_segment_start_offset_in_coords[startIndex + i + 1] - start_offset;
				}

				if constexpr (std::is_same_v<T, float>) {
					std::memcpy(s_coordinates.data() + start_offset, pathData, num_coords * sizeof(float));
				} else {
					for (std::size_t i = 0; i < num_coords; i++) {
						s_coordinates[start_offset + i] = (VGfloat)pathData[i];
					}
				}

				pathData += num_coords;
			}

			return VG_NO_ERROR;
		}

		void vgTransformPath(std::shared_ptr<Path> srcPath);
		VGboolean vgInterpolatePath(std::shared_ptr<Path> startPath,
					    std::shared_ptr<Path> endPath, VGfloat amount);
		VGfloat vgPathLength(VGint startSegment, VGint numSegments);
		void vgPointAlongPath(VGint startSegment, VGint numSegments,
				      VGfloat distance,
				      VGfloat *x, VGfloat *y,
				      VGfloat *tangentX, VGfloat *tangentY);
		void vgPathBounds(VGfloat * minX, VGfloat * minY,
				  VGfloat * width, VGfloat * height);
		void vgPathTransformedBounds(VGfloat * minX, VGfloat * minY,
					     VGfloat * width, VGfloat * height);
		virtual void vgDrawPath(const GraphicState &state, VGbitfield paintModes);
	};
}

#endif
