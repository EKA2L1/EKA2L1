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

#pragma once

#include <drivers/graphics/common.h>
#include <drivers/itc.h>

#include <map>

namespace gnuVG {
	class Shader {
	public:
		enum Values {
			gauss_krn_diameter_shift = 16,
			attrib_location_position = 0,
			attrib_location_texcoord = 1
		};
		enum Capabilities {
			// primary mode
			do_flat_color		= 0x00000000,
			do_linear_gradient	= 0x00000001,
			do_radial_gradient	= 0x00000002,
			do_pattern		= 0x00000003,
			do_texture		= 0x00000004,

			primary_mode_mask	= 0x0000000f,

			// gradient spread mode
			do_gradient_pad		= 0x00000010,
			do_gradient_repeat	= 0x00000020,
			do_gradient_reflect	= 0x00000030,

			gradient_spread_mask	= 0x00000030,

			// additional flags
			do_mask			= 0x00000100,
			do_pretranslate		= 0x00000200,
			do_horizontal_gaussian	= 0x00000400,
			do_vertical_gaussian	= 0x00000800,
			do_color_transform	= 0x01000000,

			// gaussian kernel configuration
			gauss_krn_diameter_mask = 0x00ff0000,
		};

		static eka2l1::drivers::handle create_program(eka2l1::drivers::graphics_driver *driver,
			const char *vertexshader_source,
			const char *fragmentshader_source,
			eka2l1::drivers::shader_program_metadata *metadata);

		void use_shader();

		void set_matrix(const float *mtrx);
		void set_pre_translation(const float *ptrans);

		void set_surf2paint_matrix(const float *s2p_matrix);

		void set_mask_texture(eka2l1::drivers::handle tex);

		void set_pattern_size(std::int32_t width_in_pixels, std::int32_t height_in_pixels);
		void set_pattern_matrix(const float *mtrx);
		void set_pattern_texture(eka2l1::drivers::handle tex, eka2l1::drivers::addressing_option wrap_mode);

		void set_color_transform(const float *scale,
					 const float *bias);

		void set_color(const float *clr);
		void set_linear_parameters(const float *vec);
		void set_radial_parameters(const float *vec);
		void set_color_ramp(std::uint32_t max_stops,
				    const float *offsets,
				    const float *invfactor,
				    const float *colors);

		void set_texture(eka2l1::drivers::handle tex);
		void set_texture_matrix(const float *mtrx_3by3);
		void render_triangles(std::int32_t first, std::int32_t count);
		void render_elements(eka2l1::drivers::handle index_buffer_handle, const std::size_t index_buffer_offset,
			std::int32_t nr_indices);

		void set_current_graphics_command_builder(eka2l1::drivers::graphics_command_builder &builder) {
			current_cmdbuilder_ = &builder;
		}

	private:
		friend class ShaderMan;

		float pixel_width, pixel_height;

		eka2l1::drivers::handle program_id;

		/* shader handles */
		std::int32_t position_handle;

		std::int32_t textureCoord_handle;
		std::int32_t textureMatrix_handle;
		std::int32_t textureSampler_handle;

		std::int32_t ColorHandle;
		std::int32_t Matrix;
		std::int32_t preTranslation;

		std::int32_t maskTexture;

		std::int32_t pxl_sizes;
		std::int32_t patternTexture, patternMatrix;

		std::int32_t surf2paint;

		std::int32_t ctransform_scale, ctransform_bias;

		std::int32_t linear_normal;
		std::int32_t linear_start;
		std::int32_t linear_length;

		std::int32_t radial;
		std::int32_t radius2;
		std::int32_t radial_denom;

		std::int32_t stop_colors;
		std::int32_t stop_invfactor;
		std::int32_t stop_offsets;
		std::int32_t nr_stops;

		eka2l1::drivers::graphics_command_builder *current_cmdbuilder_;

		Shader(eka2l1::drivers::graphics_driver *drv, int caps);

		std::string build_vertex_shader(int caps);
		std::string build_fragment_shader(int caps);
	};
};
