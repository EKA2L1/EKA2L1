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

/*------------------------------------------------------------------------
 *
 * VGU library for OpenVG 1.1 Reference Implementation
 * ---------------------------------------------------
 *
 * Copyright (c) 2007 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and /or associated documentation files
 * (the "Materials "), to deal in the Materials without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Materials,
 * and to permit persons to whom the Materials are furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE MATERIALS OR
 * THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//**
    * \file
    * \brief	Implementation of the VGU utility library for OpenVG
    *//*-------------------------------------------------------------------*/

#include <dispatch/libraries/vg/consts.h>
#include <stdint.h>
#include <math.h>

#include <dispatch/libraries/vg/gnuVG_emuutils.hh>
#include <dispatch/libraries/vg/gnuVG_math.hh>
#include <dispatch/libraries/vg/gnuVG_debug.hh>
#include <dispatch/libraries/vg/gnuVG_path.hh>
#include <dispatch/libraries/vg/gnuVG_context.hh>

using namespace gnuVG;

namespace eka2l1::dispatch {
	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	static void append(std::shared_ptr<Path> path, int numSegments, const VGubyte* segments, int numCoordinates, const VGfloat* coordinates)
	{
		VGPathDatatype datatype = path->get_dataType();
		VGfloat scale = path->vgGetParameterf(VG_PATH_SCALE);
		VGfloat bias = path->vgGetParameterf(VG_PATH_BIAS);

		switch(datatype)
		{
		case VG_PATH_DATATYPE_S_8:
		{
			int8_t data[26];
			for(int i=0;i<numCoordinates;i++)
				data[i] = (int8_t)floorf((coordinates[i] - bias) / scale + 0.5f);	//add 0.5 for correct rounding
			path->vgAppendPathData<std::int8_t>(numSegments, segments, data);
			break;
		}

		case VG_PATH_DATATYPE_S_16:
		{
			int16_t data[26];
			for(int i=0;i<numCoordinates;i++)
				data[i] = (int16_t)floorf((coordinates[i] - bias) / scale + 0.5f);	//add 0.5 for correct rounding
			path->vgAppendPathData<std::int16_t>(numSegments, segments, data);
			break;
		}

		case VG_PATH_DATATYPE_S_32:
		{
			int32_t data[26];
			for(int i=0;i<numCoordinates;i++)
				data[i] = (int32_t)floorf((coordinates[i] - bias) / scale + 0.5f);	//add 0.5 for correct rounding
			path->vgAppendPathData<std::int32_t>(numSegments, segments, data);
			break;
		}

		default:
		{
			float data[26];
			for(int i=0;i<numCoordinates;i++)
				data[i] = (float)((coordinates[i] - bias) / scale);
			path->vgAppendPathData<float>(numSegments, segments, data);
			break;
		}
		}
	}

	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_line_emu, VGPath path, VGfloat x0, VGfloat y0, VGfloat x1, VGfloat y1) {
		Context *context = get_active_context(sys);
		if (!context) {
			return VGU_OUT_OF_MEMORY_ERROR;
		}

		std::shared_ptr<Path> path_obj = context->get<Path>(path);
		if (path_obj == nullptr) {
			return VGU_BAD_HANDLE_ERROR;
		}

        // Clear the error state
		VGErrorCode error = context->get_error();

		static const VGubyte segments[2] = {VG_MOVE_TO | VG_ABSOLUTE, VG_LINE_TO | VG_ABSOLUTE};
		const VGfloat data[4] = {x0, y0, x1, y1};
		append(path_obj, 2, segments, 4, data);

		error = context->get_error();
		if(error == VG_BAD_HANDLE_ERROR)
			return VGU_BAD_HANDLE_ERROR;
		else if(error == VG_PATH_CAPABILITY_ERROR)
			return VGU_PATH_CAPABILITY_ERROR;

		return VGU_NO_ERROR;
	}

	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_polygon_emu, VGPath path, const VGfloat * points, VGint count, VGboolean closed)
	{
		Context *context = get_active_context(sys);
		if (!context) {
			return VGU_OUT_OF_MEMORY_ERROR;
		}

		VGErrorCode error = context->get_error();	//clear the error state
		if(!points || count <= 0)
			return VGU_ILLEGAL_ARGUMENT_ERROR;

		std::shared_ptr<Path> path_obj = context->get<Path>(path);
		if (path_obj == nullptr) {
			return VGU_BAD_HANDLE_ERROR;
		}

		VGubyte segments[1] = {VG_MOVE_TO | VG_ABSOLUTE};
		VGfloat data[2];
		for(int i=0;i<count;i++)
		{
			data[0] = points[i*2+0];
			data[1] = points[i*2+1];
			append(path_obj, 1, segments, 2, data);
			segments[0] = VG_LINE_TO | VG_ABSOLUTE;
		}
		if(closed)
		{
			segments[0] = VG_CLOSE_PATH;
			append(path_obj, 1, segments, 0, data);
		}

		error = context->get_error();
		if(error == VG_BAD_HANDLE_ERROR)
			return VGU_BAD_HANDLE_ERROR;
		else if(error == VG_PATH_CAPABILITY_ERROR)
			return VGU_PATH_CAPABILITY_ERROR;
		return VGU_NO_ERROR;
	}

	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_rect_emu, VGPath path, VGfloat x, VGfloat y, VGfloat width, VGfloat height)
	{
		Context *context = get_active_context(sys);
		if (!context) {
			return VGU_OUT_OF_MEMORY_ERROR;
		}

		VGErrorCode error = context->get_error();	//clear the error state
		if(width <= 0 || height <= 0)
			return VGU_ILLEGAL_ARGUMENT_ERROR;

		std::shared_ptr<Path> path_obj = context->get<Path>(path);
		if (path_obj == nullptr) {
			return VGU_BAD_HANDLE_ERROR;
		}

		static const VGubyte segments[5] = {VG_MOVE_TO | VG_ABSOLUTE,
							VG_HLINE_TO | VG_ABSOLUTE,
							VG_VLINE_TO | VG_ABSOLUTE,
							VG_HLINE_TO | VG_ABSOLUTE,
							VG_CLOSE_PATH};
		const VGfloat data[5] = {x, y, x + width, y + height, x};
		append(path_obj, 5, segments, 5, data);

		error = context->get_error();
		if(error == VG_BAD_HANDLE_ERROR)
			return VGU_BAD_HANDLE_ERROR;
		else if(error == VG_PATH_CAPABILITY_ERROR)
			return VGU_PATH_CAPABILITY_ERROR;
		return VGU_NO_ERROR;
	}

	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_round_rect_emu, VGPath path, VGfloat x, VGfloat y, VGfloat width, VGfloat height, VGfloat arcWidth, VGfloat arcHeight)
	{
		Context *context = get_active_context(sys);
		if (!context) {
			return VGU_OUT_OF_MEMORY_ERROR;
		}

		VGErrorCode error = context->get_error();	//clear the error state
		if(width <= 0 || height <= 0)
			return VGU_ILLEGAL_ARGUMENT_ERROR;

		std::shared_ptr<Path> path_obj = context->get<Path>(path);
		if (path_obj == nullptr) {
			return VGU_BAD_HANDLE_ERROR;
		}

		arcWidth = GNUVG_CLAMP(arcWidth, 0.0f, width);
		arcHeight = GNUVG_CLAMP(arcHeight, 0.0f, height);

		static const VGubyte segments[10] = {VG_MOVE_TO | VG_ABSOLUTE,
							VG_HLINE_TO | VG_ABSOLUTE,
							VG_SCCWARC_TO | VG_ABSOLUTE,
							VG_VLINE_TO | VG_ABSOLUTE,
							VG_SCCWARC_TO | VG_ABSOLUTE,
							VG_HLINE_TO | VG_ABSOLUTE,
							VG_SCCWARC_TO | VG_ABSOLUTE,
							VG_VLINE_TO | VG_ABSOLUTE,
							VG_SCCWARC_TO | VG_ABSOLUTE,
							VG_CLOSE_PATH};

		const VGfloat data[26] = {x + arcWidth/2, y,
					x + width - arcWidth/2,
					arcWidth/2, arcHeight/2, 0, x + width, y + arcHeight/2,
					y + height - arcHeight/2,
					arcWidth/2, arcHeight/2, 0, x + width - arcWidth/2, y + height,
					x + arcWidth/2,
					arcWidth/2, arcHeight/2, 0, x, y + height - arcHeight/2,
					y + arcHeight/2,
					arcWidth/2, arcHeight/2, 0, x + arcWidth/2, y};
		append(path_obj, 10, segments, 26, data);

		error = context->get_error();
		if(error == VG_BAD_HANDLE_ERROR)
			return VGU_BAD_HANDLE_ERROR;
		else if(error == VG_PATH_CAPABILITY_ERROR)
			return VGU_PATH_CAPABILITY_ERROR;
		return VGU_NO_ERROR;
	}

	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_ellipse_emu, VGPath path, VGfloat cx, VGfloat cy, VGfloat width, VGfloat height)
	{
		Context *context = get_active_context(sys);
		if (!context) {
			return VGU_OUT_OF_MEMORY_ERROR;
		}

		VGErrorCode error = context->get_error();	//clear the error state
		if(width <= 0 || height <= 0)
			return VGU_ILLEGAL_ARGUMENT_ERROR;

		std::shared_ptr<Path> path_obj = context->get<Path>(path);
		if (path_obj == nullptr) {
			return VGU_BAD_HANDLE_ERROR;
		}

		static const VGubyte segments[4] = {VG_MOVE_TO | VG_ABSOLUTE,
							VG_SCCWARC_TO | VG_ABSOLUTE,
							VG_SCCWARC_TO | VG_ABSOLUTE,
							VG_CLOSE_PATH};
		const VGfloat data[12] = {cx + width/2, cy,
					width/2, height/2, 0, cx - width/2, cy,
					width/2, height/2, 0, cx + width/2, cy};
		append(path_obj, 4, segments, 12, data);

		error = context->get_error();
		if(error == VG_BAD_HANDLE_ERROR)
			return VGU_BAD_HANDLE_ERROR;
		else if(error == VG_PATH_CAPABILITY_ERROR)
			return VGU_PATH_CAPABILITY_ERROR;
		return VGU_NO_ERROR;
	}

	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_arc_emu, VGPath path, VGfloat x, VGfloat y, VGfloat width, VGfloat height, VGfloat startAngle, VGfloat angleExtent, VGUArcType arcType)
	{
		Context *context = get_active_context(sys);
		if (!context) {
			return VGU_OUT_OF_MEMORY_ERROR;
		}

		VGErrorCode error = context->get_error();	//clear the error state
		if((arcType != VGU_ARC_OPEN && arcType != VGU_ARC_CHORD && arcType != VGU_ARC_PIE) || width <= 0.0f || height <= 0.0f)
			return VGU_ILLEGAL_ARGUMENT_ERROR;

		std::shared_ptr<Path> path_obj = context->get<Path>(path);
		if (path_obj == nullptr) {
			return VGU_BAD_HANDLE_ERROR;
		}

		startAngle = GNUVG_DEG_TO_RAD(startAngle);
		angleExtent = GNUVG_DEG_TO_RAD(angleExtent);

		VGfloat w = width/2.0f;
		VGfloat h = height/2.0f;

		VGubyte segments[1];
		VGfloat data[5];

		segments[0] = VG_MOVE_TO | VG_ABSOLUTE;
		data[0] = x + w * (VGfloat)cos(startAngle);
		data[1] = y + h * (VGfloat)sin(startAngle);
		append(path_obj, 1, segments, 2, data);

		data[0] = w;
		data[1] = h;
		data[2] = 0.0f;
		VGfloat endAngle = startAngle + angleExtent;
		if(angleExtent >= 0.0f)
		{
			segments[0] = VG_SCCWARC_TO | VG_ABSOLUTE;
			for(VGfloat a = startAngle + M_PI;a < endAngle; a += M_PI)
			{
				data[3] = x + w * (VGfloat)cos(a);
				data[4] = y + h * (VGfloat)sin(a);
				append(path_obj, 1, segments, 5, data);
			}
		}
		else
		{
			segments[0] = VG_SCWARC_TO | VG_ABSOLUTE;
			for(VGfloat a = startAngle - M_PI;a > endAngle; a -= M_PI)
			{
				data[3] = x + w * (VGfloat)cos(a);
				data[4] = y + h * (VGfloat)sin(a);
				append(path_obj, 1, segments, 5, data);
			}
		}
		data[3] = x + w * (VGfloat)cos(endAngle);
		data[4] = y + h * (VGfloat)sin(endAngle);
		append(path_obj, 1, segments, 5, data);

		if(arcType == VGU_ARC_CHORD)
		{
			segments[0] = VG_CLOSE_PATH;
			append(path_obj, 1, segments, 0, data);
		}
		else if(arcType == VGU_ARC_PIE)
		{
			segments[0] = VG_LINE_TO | VG_ABSOLUTE;
			data[0] = x;
			data[1] = y;
			append(path_obj, 1, segments, 2, data);
			segments[0] = VG_CLOSE_PATH;
			append(path_obj, 1, segments, 0, data);
		}

		error = context->get_error();
		if(error == VG_BAD_HANDLE_ERROR)
			return VGU_BAD_HANDLE_ERROR;
		else if(error == VG_PATH_CAPABILITY_ERROR)
			return VGU_PATH_CAPABILITY_ERROR;
		return VGU_NO_ERROR;
	}

	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_compute_warp_square_to_quad_emu, VGfloat dx0, VGfloat dy0, VGfloat dx1, VGfloat dy1, VGfloat dx2, VGfloat dy2, VGfloat dx3, VGfloat dy3, VGfloat * matrix)
	{
		if(!matrix)
			return VGU_ILLEGAL_ARGUMENT_ERROR;

		//from Heckbert:Fundamentals of Texture Mapping and Image Warping
		//Note that his mapping of vertices is different from OpenVG's
		//(0,0) => (dx0,dy0)
		//(1,0) => (dx1,dy1)
		//(0,1) => (dx2,dy2)
		//(1,1) => (dx3,dy3)

		VGfloat diffx1 = dx1 - dx3;
		VGfloat diffy1 = dy1 - dy3;
		VGfloat diffx2 = dx2 - dx3;
		VGfloat diffy2 = dy2 - dy3;

		VGfloat det = diffx1*diffy2 - diffx2*diffy1;
		if(det == 0.0f)
			return VGU_BAD_WARP_ERROR;

		VGfloat sumx = dx0 - dx1 + dx3 - dx2;
		VGfloat sumy = dy0 - dy1 + dy3 - dy2;

		if(sumx == 0.0f && sumy == 0.0f)
		{	//affine mapping
			matrix[0] = dx1 - dx0;
			matrix[1] = dy1 - dy0;
			matrix[2] = 0.0f;
			matrix[3] = dx3 - dx1;
			matrix[4] = dy3 - dy1;
			matrix[5] = 0.0f;
			matrix[6] = dx0;
			matrix[7] = dy0;
			matrix[8] = 1.0f;
			return VGU_NO_ERROR;
		}

		VGfloat oodet = 1.0f / det;
		VGfloat g = (sumx*diffy2 - diffx2*sumy) * oodet;
		VGfloat h = (diffx1*sumy - sumx*diffy1) * oodet;

		matrix[0] = dx1-dx0+g*dx1;
		matrix[1] = dy1-dy0+g*dy1;
		matrix[2] = g;
		matrix[3] = dx2-dx0+h*dx2;
		matrix[4] = dy2-dy0+h*dy2;
		matrix[5] = h;
		matrix[6] = dx0;
		matrix[7] = dy0;
		matrix[8] = 1.0f;
		return VGU_NO_ERROR;
	}

	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_compute_warp_quad_to_square_emu, VGfloat sx0, VGfloat sy0, VGfloat sx1, VGfloat sy1, VGfloat sx2, VGfloat sy2, VGfloat sx3, VGfloat sy3, VGfloat * matrix)
	{
		if(!matrix)
			return VGU_ILLEGAL_ARGUMENT_ERROR;

		VGfloat mat[9];
		VGUErrorCode ret = vgu_compute_warp_square_to_quad_emu(sys, sx0, sy0, sx1, sy1, sx2, sy2, sx3, sy3, mat);
		if(ret == VGU_BAD_WARP_ERROR)
			return VGU_BAD_WARP_ERROR;
		Matrix m(mat[0], mat[3], mat[6],
			mat[1], mat[4], mat[7],
			mat[2], mat[5], mat[8]);
		bool nonsingular = m.invert();
		if(!nonsingular)
			return VGU_BAD_WARP_ERROR;
		m.get(matrix);
		return VGU_NO_ERROR;
	}

	/*-------------------------------------------------------------------*/
	/*!
	* \brief
	* \param
	* \return
	* \note
	*/
	/*-------------------------------------------------------------------*/

	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_compute_warp_quad_to_quad_emu, VGfloat dx0, VGfloat dy0, VGfloat dx1, VGfloat dy1, VGfloat dx2, VGfloat dy2, VGfloat dx3, VGfloat dy3, VGfloat sx0, VGfloat sy0, VGfloat sx1, VGfloat sy1, VGfloat sx2, VGfloat sy2, VGfloat sx3, VGfloat sy3, VGfloat * matrix)
	{
		if(!matrix)
			return VGU_ILLEGAL_ARGUMENT_ERROR;

		VGfloat qtos[9];
		VGUErrorCode ret1 = vgu_compute_warp_quad_to_square_emu(sys, sx0, sy0, sx1, sy1, sx2, sy2, sx3, sy3, qtos);
		if(ret1 == VGU_BAD_WARP_ERROR)
			return VGU_BAD_WARP_ERROR;

		VGfloat stoq[9];
		VGUErrorCode ret2 = vgu_compute_warp_square_to_quad_emu(sys, dx0, dy0, dx1, dy1, dx2, dy2, dx3, dy3, stoq);
		if(ret2 == VGU_BAD_WARP_ERROR)
			return VGU_BAD_WARP_ERROR;

		Matrix m1(qtos[0], qtos[3], qtos[6],
			qtos[1], qtos[4], qtos[7],
			qtos[2], qtos[5], qtos[8]);
		Matrix m2(stoq[0], stoq[3], stoq[6],
			stoq[1], stoq[4], stoq[7],
			stoq[2], stoq[5], stoq[8]);
		Matrix r;
		r.multiply(m2, m1);
		r.get(matrix);
		return VGU_NO_ERROR;
	}

}