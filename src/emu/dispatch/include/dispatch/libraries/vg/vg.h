#pragma once

#include <dispatch/libraries/vg/consts.h>
#include <dispatch/def.h>

namespace eka2l1::dispatch {
    BRIDGE_FUNC_LIBRARY(void, vg_gaussian_blur_emu, VGImage dst, VGImage src, VGfloat stdDeviationX, VGfloat stdDeviationY, VGTilingMode tilingMode);
	BRIDGE_FUNC_LIBRARY(VGErrorCode, vg_get_error_emu);
	BRIDGE_FUNC_LIBRARY(void, vg_setf_emu, VGParamType type, VGfloat value);
	BRIDGE_FUNC_LIBRARY(void, vg_seti_emu, VGParamType type, VGint value);
    BRIDGE_FUNC_LIBRARY(void, vg_setfv_emu, VGParamType type, VGint count, const VGfloat * values);
    BRIDGE_FUNC_LIBRARY(void, vg_setiv_emu, VGParamType type, VGint count, const VGint * values);
	BRIDGE_FUNC_LIBRARY(VGfloat, vg_getf_emu, VGParamType type);
	BRIDGE_FUNC_LIBRARY(VGint, vg_geti_emu, VGParamType type);
	BRIDGE_FUNC_LIBRARY(VGint, vg_get_vector_size_emu, VGParamType type);
	BRIDGE_FUNC_LIBRARY(void, vg_getfv_emu, VGParamType type, VGint count, VGfloat * values);
	BRIDGE_FUNC_LIBRARY(void, vg_getiv_emu, VGParamType type, VGint count, VGint * values);
    BRIDGE_FUNC_LIBRARY(void, vg_set_parameterf_emu, VGHandle object, VGint paramType, VGfloat value);
	BRIDGE_FUNC_LIBRARY(void, vg_set_parameteri_emu, VGHandle object, VGint paramType, VGint value);
	BRIDGE_FUNC_LIBRARY(void, vg_set_parameterfv_emu, VGHandle object, VGint paramType,
		VGint count, const VGfloat * values);
	BRIDGE_FUNC_LIBRARY(void, vg_set_parameteriv_emu, VGHandle object, VGint paramType,
		VGint count, const VGint * values);
	BRIDGE_FUNC_LIBRARY(VGfloat, vg_get_parameterf_emu, VGHandle object, VGint paramType);
	BRIDGE_FUNC_LIBRARY(VGint, vg_get_parameteri_emu, VGHandle object, VGint paramType);
	BRIDGE_FUNC_LIBRARY(VGint, vg_get_paremeter_vector_size_emu, VGHandle object, VGint paramType);
	BRIDGE_FUNC_LIBRARY(void, vg_get_parameterfv_emu, VGHandle object, VGint paramType,
		VGint count, VGfloat * values);
	BRIDGE_FUNC_LIBRARY(void, vg_get_parameteriv_emu, VGHandle object, VGint paramType,
		VGint count, VGint * values);
	BRIDGE_FUNC_LIBRARY(void, vg_load_identity_emu);
	BRIDGE_FUNC_LIBRARY(void, vg_load_matrix_emu, const VGfloat * m);
	BRIDGE_FUNC_LIBRARY(void, vg_get_matrix_emu, VGfloat * m);
	BRIDGE_FUNC_LIBRARY(void, vg_mult_matrix_emu, const VGfloat * m);
	BRIDGE_FUNC_LIBRARY(void, vg_translate_emu, VGfloat tx, VGfloat ty);
	BRIDGE_FUNC_LIBRARY(void, vg_scale_emu, VGfloat sx, VGfloat sy);
	BRIDGE_FUNC_LIBRARY(void, vg_shear_emu, VGfloat shx, VGfloat shy);
	BRIDGE_FUNC_LIBRARY(void, vg_rotate_emu, VGfloat angle);
	BRIDGE_FUNC_LIBRARY(void, vg_clear_emu, VGint x, VGint y, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(VGImage, vg_create_image_emu, VGImageFormat format, VGint width, VGint height,
		VGbitfield allowedQuality);
	BRIDGE_FUNC_LIBRARY(void, vg_destroy_image_emu, VGImage image);
	BRIDGE_FUNC_LIBRARY(void, vg_clear_image_emu, VGImage image, VGint x, VGint y, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(void, vg_image_subdata_emu, VGImage image, const void * data, VGint dataStride,
		VGImageFormat dataFormat, VGint x, VGint y, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(void, vg_get_image_subdata_emu, VGImage image, void * data, VGint dataStride,
		VGImageFormat dataFormat, VGint x, VGint y, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(VGImage, vg_child_image_emu, VGImage parent, VGint x, VGint y, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(VGImage, vg_get_parent_emu, VGImage image);
	BRIDGE_FUNC_LIBRARY(void, vg_draw_image_emu, VGImage image);
	BRIDGE_FUNC_LIBRARY(void, vg_copy_image_emu, VGImage dst, VGint dx, VGint dy,
		VGImage src, VGint sx, VGint sy, VGint width, VGint height, VGboolean dither);
	BRIDGE_FUNC_LIBRARY(void, vg_set_pixels_emu, VGint dx, VGint dy, VGImage src, VGint sx, VGint sy,
		VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(void, vg_get_pixels_emu, VGImage dst, VGint dx, VGint dy,
		VGint sx, VGint sy, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(void, vg_write_pixels_emu, const void * data, VGint dataStride, VGImageFormat dataFormat,
		VGint dx, VGint dy, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(void, vg_read_pixels_emu, void * data, VGint dataStride,
		VGImageFormat dataFormat, VGint sx, VGint sy, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(void, vg_copy_pixels_emu, VGint dx, VGint dy, VGint sx, VGint sy, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(VGPaint, vg_create_paint_emu);
	BRIDGE_FUNC_LIBRARY(void, vg_destroy_paint_emu, VGPaint paint);
	BRIDGE_FUNC_LIBRARY(void, vg_set_paint_emu, VGPaint paint, VGbitfield paintModes);
	BRIDGE_FUNC_LIBRARY(VGPaint, vg_get_paint_emu, VGPaintMode paintMode);
	BRIDGE_FUNC_LIBRARY(void, vg_set_color_emu, VGPaint paint, VGuint rgba);
	BRIDGE_FUNC_LIBRARY(VGuint, vg_get_color_emu, VGPaint paint);
	BRIDGE_FUNC_LIBRARY(void, vg_paint_pattern_emu, VGPaint paint, VGImage pattern);
	BRIDGE_FUNC_LIBRARY(VGMaskLayer, vg_create_mask_layer_emu, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(void, vg_destroy_mask_layer_emu, VGMaskLayer mskl);
	BRIDGE_FUNC_LIBRARY(void, vg_fill_mask_layer_emu, VGMaskLayer mskl, VGint x, VGint y,
		VGint width, VGint height, VGfloat value);
	BRIDGE_FUNC_LIBRARY(void, vg_copy_mask_emu, VGMaskLayer mskl, VGint dx, VGint dy,
		VGint sx, VGint sy, VGint width, VGint height);
	BRIDGE_FUNC_LIBRARY(void, vg_mask_emu, VGHandle mask, VGMaskOperation operation, VGint x, VGint y,
		VGint width, VGint height);
	/*BRIDGE_FUNC_LIBRARY(void, vg_render_to_mask_emu, VGPath path, VGbitfield paintModes,
		VGMaskOperation operation);*/
	BRIDGE_FUNC_LIBRARY(VGPath, vg_create_path_emu, VGint pathFormat, VGPathDatatype datatype,
		VGfloat scale, VGfloat bias, VGint segmentCapacityHint, VGint coordCapacityHint,
		VGbitfield capabilities);
	BRIDGE_FUNC_LIBRARY(void, vg_destroy_path_emu, VGPath path);
	BRIDGE_FUNC_LIBRARY(void, vg_clear_path_emu, VGPath path, VGbitfield capabilities);
	//BRIDGE_FUNC_LIBRARY(void, vg_remove_path_capabilities_emu, VGPath path, VGbitfield capabilities);
	BRIDGE_FUNC_LIBRARY(VGbitfield, vg_get_path_capabilities_emu, VGPath path);
	BRIDGE_FUNC_LIBRARY(void, vg_append_path_emu, VGPath dstPath, VGPath srcPath);
	BRIDGE_FUNC_LIBRARY(void, vg_append_path_data_emu, VGPath path, VGint numSegments,
		const VGubyte *pathSegments, const void *pathData);
	/*BRIDGE_FUNC_LIBRARY(void, vg_modify_path_coords_emu, VGPath dstPath, VGint startIndex,
		VGint numSegments, const void * pathData);*/
	BRIDGE_FUNC_LIBRARY(void, vg_transform_path_emu, VGPath dstPath, VGPath srcPath);
	BRIDGE_FUNC_LIBRARY(void, vg_transform_path_emu, VGPath dstPath, VGPath srcPath);
	BRIDGE_FUNC_LIBRARY(VGboolean, vg_interpolate_path_emu, VGPath dstPath,
		VGPath startPath, VGPath endPath, VGfloat amount);
	BRIDGE_FUNC_LIBRARY(VGfloat, vg_path_length_emu, VGPath path, VGint startSegment, VGint numSegments);
	BRIDGE_FUNC_LIBRARY(void, vg_point_along_path_emu, VGPath path, VGint startSegment, VGint numSegments,
		VGfloat distance, VGfloat * x, VGfloat * y, VGfloat * tangentX, VGfloat * tangentY);
	BRIDGE_FUNC_LIBRARY(void, vg_path_bounds_emu, VGPath path, VGfloat *minX, VGfloat *minY,
		VGfloat *width, VGfloat *height);
	BRIDGE_FUNC_LIBRARY(void, vg_path_transformed_bounds_emu, VGPath path, VGfloat * minX, VGfloat * minY,
		VGfloat * width, VGfloat * height);
	BRIDGE_FUNC_LIBRARY(VGFont, vg_create_font_emu, VGint glyphCapacityHint);
	BRIDGE_FUNC_LIBRARY(void, vg_destroy_font_emu, VGFont font);
	BRIDGE_FUNC_LIBRARY(void, vg_set_glyph_to_path_emu, VGFont font, VGuint glyphIndex, VGPath path,
		VGboolean isHinted, const VGfloat *glyphOrigin, const VGfloat *escapement);
	BRIDGE_FUNC_LIBRARY(void, vg_set_glyph_to_image_emu, VGFont font, VGuint glyphIndex,
		VGImage image, const VGfloat *glyphOrigin, const VGfloat *escapement);
	BRIDGE_FUNC_LIBRARY(void, vg_clear_glyph_emu, VGFont font, VGuint glyphIndex);
	BRIDGE_FUNC_LIBRARY(void, vg_draw_glyph_emu, VGFont font, VGuint glyphIndex, VGbitfield paintModes,
		VGboolean allowAutoHinting);
	BRIDGE_FUNC_LIBRARY(void, vg_draw_glyphs_emu, VGFont font, VGint glyphCount, const VGuint *glyphIndices,
		const VGfloat *adjustments_x, const VGfloat *adjustments_y, VGbitfield paintModes,
		VGboolean allowAutoHinting);
	BRIDGE_FUNC_LIBRARY(address, vg_get_string_emu, std::uint32_t pname);
	BRIDGE_FUNC_LIBRARY(void, vg_draw_path_emu, VGPath path, VGbitfield paintModes);
	BRIDGE_FUNC_LIBRARY(void, vg_modify_path_coords_emu, VGPath dstPath, VGint startIndex,
		VGint numSegments, const void *pathData);

        // VGU
	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_line_emu, VGPath path, VGfloat x0, VGfloat y0, VGfloat x1, VGfloat y1);
	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_polygon_emu, VGPath path, const VGfloat *points, VGint count, VGboolean closed);
	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_rect_emu, VGPath path, VGfloat x, VGfloat y, VGfloat width, VGfloat height);
	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_round_rect_emu, VGPath path, VGfloat x, VGfloat y, VGfloat width, VGfloat height, VGfloat arcWidth, VGfloat arcHeight);
	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_ellipse_emu, VGPath path, VGfloat cx, VGfloat cy, VGfloat width, VGfloat height);
	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_arc_emu, VGPath path, VGfloat x, VGfloat y, VGfloat width, VGfloat height, VGfloat startAngle, VGfloat angleExtent, VGUArcType arcType);
	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_compute_warp_quad_to_square_emu, VGfloat sx0, VGfloat sy0, VGfloat sx1, VGfloat sy1, VGfloat sx2, VGfloat sy2, VGfloat sx3, VGfloat sy3, VGfloat * matrix);
	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_compute_warp_square_to_quad_emu, VGfloat dx0, VGfloat dy0, VGfloat dx1, VGfloat dy1, VGfloat dx2, VGfloat dy2, VGfloat dx3, VGfloat dy3, VGfloat *matrix);
	BRIDGE_FUNC_LIBRARY(VGUErrorCode, vgu_compute_warp_quad_to_quad_emu, VGfloat dx0, VGfloat dy0, VGfloat dx1, VGfloat dy1, VGfloat dx2, VGfloat dy2, VGfloat dx3, VGfloat dy3, VGfloat sx0, VGfloat sy0, VGfloat sx1, VGfloat sy1, VGfloat sx2, VGfloat sy2, VGfloat sx3, VGfloat sy3, VGfloat *matrix);
}