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

#include <sstream>
#include <dispatch/libraries/vg/gnuVG_shader.hh>
#include <dispatch/libraries/vg/gnuVG_gaussianblur.hh>

#include <drivers/graphics/graphics.h>

namespace gnuVG {
	void Shader::use_shader() {
		current_cmdbuilder_->use_program(program_id);
	}

	void Shader::set_matrix(const float *m) {
		current_cmdbuilder_->set_dynamic_uniform(Matrix, eka2l1::drivers::shader_var_type::mat4,
			m, 64);
	}

	void Shader::set_pre_translation(const float *ptrans) {
		current_cmdbuilder_->set_dynamic_uniform(preTranslation, eka2l1::drivers::shader_var_type::vec2,
			ptrans, 2 * sizeof(float));
	}

	void Shader::set_surf2paint_matrix(const float *s2p_matrix) {
		current_cmdbuilder_->set_dynamic_uniform(surf2paint, eka2l1::drivers::shader_var_type::mat4,
			s2p_matrix, 64);
	}

	void Shader::set_mask_texture(eka2l1::drivers::handle tex) {
		current_cmdbuilder_->bind_texture(tex, 1);
		current_cmdbuilder_->set_texture_for_shader(1, maskTexture, eka2l1::drivers::shader_module_type::fragment);
	}

	void Shader::set_pattern_size(std::int32_t width_in_pixels, std::int32_t height_in_pixels) {
		auto pixel_width = (float)1.0 / (float)width_in_pixels;
		auto pixel_height = (float)1.0 / (float)height_in_pixels;

		float pxs[] = {
			pixel_width,
			pixel_height,
			pixel_width * (float)0.5,
			pixel_height * (float)0.5
		};

		current_cmdbuilder_->set_dynamic_uniform(pxl_sizes, eka2l1::drivers::shader_var_type::vec4,
			pxs, 4 * sizeof(float));
	}

	void Shader::set_pattern_matrix(const float *mtrx) {
		current_cmdbuilder_->set_dynamic_uniform(patternMatrix, eka2l1::drivers::shader_var_type::mat4,
			mtrx, 64);
	}

	void Shader::set_pattern_texture(eka2l1::drivers::handle tex, eka2l1::drivers::addressing_option wrap_mod) {
		current_cmdbuilder_->set_texture_addressing_mode(tex, eka2l1::drivers::addressing_direction::s, wrap_mod);
		current_cmdbuilder_->set_texture_addressing_mode(tex, eka2l1::drivers::addressing_direction::t, wrap_mod);

		current_cmdbuilder_->bind_texture(tex, 2);
		current_cmdbuilder_->set_texture_for_shader(2, patternTexture, eka2l1::drivers::shader_module_type::fragment);
	}

	void Shader::set_color_transform(const float *scale, const float *bias) {
		current_cmdbuilder_->set_dynamic_uniform(ctransform_scale, eka2l1::drivers::shader_var_type::mat4,
			scale, 64);
		current_cmdbuilder_->set_dynamic_uniform(ctransform_bias, eka2l1::drivers::shader_var_type::mat4,
			bias, 64);
	}

	void Shader::set_color(const float *clr) {
		current_cmdbuilder_->set_dynamic_uniform(ColorHandle, eka2l1::drivers::shader_var_type::vec4,
			clr, 4 * sizeof(float));
	}

	void Shader::set_linear_parameters(const float *vec) {
		current_cmdbuilder_->set_dynamic_uniform(linear_start, eka2l1::drivers::shader_var_type::vec2,
			&(vec[0]), 2 * sizeof(float));
		current_cmdbuilder_->set_dynamic_uniform(linear_normal, eka2l1::drivers::shader_var_type::vec2,
			&(vec[2]), 2 * sizeof(float));
		current_cmdbuilder_->set_dynamic_uniform(linear_length, eka2l1::drivers::shader_var_type::real,
			&(vec[4]), sizeof(float));
	}

	void Shader::set_radial_parameters(const float *vec) {
		current_cmdbuilder_->set_dynamic_uniform(radial, eka2l1::drivers::shader_var_type::vec4,
			&(vec[0]), 4 * sizeof(float));
		current_cmdbuilder_->set_dynamic_uniform(radius2, eka2l1::drivers::shader_var_type::real,
			&(vec[4]), sizeof(float));
		current_cmdbuilder_->set_dynamic_uniform(radial_denom, eka2l1::drivers::shader_var_type::real,
			&(vec[5]), sizeof(float));
	}

	void Shader::set_color_ramp(std::uint32_t max_stops,
				    const float *offsets,
				    const float *invfactor,
				    const float *colors) {
		current_cmdbuilder_->set_dynamic_uniform(stop_offsets, eka2l1::drivers::shader_var_type::real,
			offsets, max_stops * sizeof(float));
		current_cmdbuilder_->set_dynamic_uniform(stop_invfactor, eka2l1::drivers::shader_var_type::real,
			invfactor, max_stops * sizeof(float));
		current_cmdbuilder_->set_dynamic_uniform(stop_colors, eka2l1::drivers::shader_var_type::vec4,
			colors, max_stops * 4 * sizeof(float));
		current_cmdbuilder_->set_dynamic_uniform(nr_stops, eka2l1::drivers::shader_var_type::integer,
			&max_stops, sizeof(std::int32_t));
	}

	void Shader::set_texture_matrix(const float *mtrx) {
		current_cmdbuilder_->set_dynamic_uniform(textureMatrix_handle, eka2l1::drivers::shader_var_type::mat3,
			mtrx, 36);
	}

	void Shader::set_texture(eka2l1::drivers::handle tex) {
		current_cmdbuilder_->bind_texture(tex, 3);
		current_cmdbuilder_->set_texture_for_shader(3, textureSampler_handle, eka2l1::drivers::shader_module_type::fragment);
	}

	void Shader::render_triangles(std::int32_t first, std::int32_t count) {
		current_cmdbuilder_->draw_arrays(eka2l1::drivers::graphics_primitive_mode::triangles, first, count, 1);
		current_cmdbuilder_ = nullptr;
	}

	void Shader::render_elements(eka2l1::drivers::handle index_buffer_handle, const std::size_t index_buffer_offset, 
		std::int32_t nr_indices) {
		current_cmdbuilder_->set_index_buffer(index_buffer_handle);
		current_cmdbuilder_->draw_indexed(eka2l1::drivers::graphics_primitive_mode::triangles, nr_indices,
			eka2l1::drivers::data_format::uint, static_cast<int>(index_buffer_offset), 0);

		// Clear it, for now to notice a render pass end
		current_cmdbuilder_ = nullptr;
	}

	std::string  Shader::build_vertex_shader(int caps) {
		std::stringstream vshad;

		vshad <<
			"uniform mat4 modelview_projection;\n"
			"layout (location = 0) in vec2 v_position;\n"
			;

		if(caps & do_pretranslate)
			vshad <<
				"uniform vec2 pre_translation;\n"
				;

		if(caps & do_mask)
			vshad <<
				"out vec2 v_maskCoord;\n"
				;

		if((caps & primary_mode_mask) == do_pattern)
			vshad <<
				"uniform mat4 p_projection;\n"
				"out vec4 p_textureCoord;\n"
				;

		if ((caps & primary_mode_mask) == do_texture)
			vshad <<
				"layout (location = 1) in vec2 a_textureCoord;\n"
				"uniform mat3 u_textureMatrix;\n"
				"out vec2 v_textureCoord;\n"
				;

		if(caps & gradient_spread_mask) {
			// gradient coordinates are in paint space - use surface2paint conversion matrix
			vshad << "uniform mat4 surf2paint;\n";
			vshad << "out vec2 gradient_coord;\n";
		}

		vshad <<
			"void main() {\n"
			;

		if(caps & do_pretranslate)
			vshad <<
			"  gl_Position = modelview_projection * vec4(v_position + pre_translation, 0.0, 1.0);\n";
		else
			vshad <<
			"  gl_Position = modelview_projection * vec4(v_position, 0.0, 1.0);\n";

		if(caps & do_mask)
			vshad <<
				"  v_maskCoord = vec2(gl_Position.x * 0.5 + 0.5, gl_Position.y * 0.5 + 0.5);\n"
				;

		if((caps & primary_mode_mask) == do_pattern)
			vshad <<
				"  p_textureCoord = p_projection * vec4(gl_Position.xy, 0.0, 1.0);\n"
				;

		if((caps & primary_mode_mask) == do_texture)
			vshad <<
				"  v_textureCoord = a_textureCoord;\n"
//				"  v_textureCoord = a_textureCoord.xy;\n"
				;

		if(caps & gradient_spread_mask)
			vshad <<
				"  vec4 gc_tmp = surf2paint * gl_Position;\n"
				"  gradient_coord = vec2(gc_tmp.x, gc_tmp.y);\n"
				;


		vshad <<
			"}\n";

		return vshad.str();
	}

	std::string Shader::build_fragment_shader(int caps) {
		std::stringstream fshad;

		bool horizontal_gauss = false;
		bool do_gauss = false;

		if(caps & do_horizontal_gaussian) {
			horizontal_gauss = true;
			do_gauss = true;
		} else if(caps & do_vertical_gaussian)
			do_gauss = true;

		fshad <<
			"precision highp float;\n"
			;

		fshad << "out vec4 o_color;\n";

		if (caps & gradient_spread_mask)
			fshad << "in vec2 gradient_coord;\n";

		if(caps & do_mask)
			fshad <<
				"in vec2 v_maskCoord;\n"
				"uniform sampler2D m_texture;\n";

		if(caps & do_color_transform)
			fshad <<
				"uniform vec4 ctransform_scale;\n"
				"uniform vec4 ctransform_bias;\n";

		auto primary_mode = caps & primary_mode_mask;
		if(primary_mode == do_linear_gradient ||
		   primary_mode == do_radial_gradient) {
			fshad <<
				"uniform vec4 stop_colors[32];\n" // stop position, R, G, B, A
				"uniform float stop_invfactor[32];\n" // used to calculate distance between stops
				"uniform float stop_offsets[32];\n" // stop position, R, G, B, A
				"uniform int nr_stops;\n"       // number of active stops, max 32
				"uniform int color_ramp_mode;\n"   // 0 = pad, 1 = repeat, 2 = mirror
				;

			if(primary_mode == do_linear_gradient) {
				fshad <<
					"uniform vec2 linear_normal;\n" // normal - for calculating distance in linear gradient
					"uniform vec2 linear_start;\n"     // linear gradient (x0, y0)
					"uniform float linear_length;\n"  // length of gradient
					;
			} else {
				fshad <<
					"uniform vec4 radial;\n"    // (focus.x, f.y, focus.x - center.x, f.y - c.y)
					"uniform float radius2;\n"  // radius ^ 2
					"uniform float radial_denom;\n"  // 1 / (r^2 − (fx'^2 + fy'^2))
					;
			}
		} else if(primary_mode == do_pattern)
			fshad <<
				"in vec4 p_textureCoord;\n"
				"uniform sampler2D p_texture;\n";
		else // flat color
			fshad <<
				"uniform vec4 v_color;\n"
				;

		if(primary_mode == do_texture)
			fshad <<
				"in vec2 v_textureCoord;\n"
				"uniform sampler2D u_textureSampler;\n";

		if(do_gauss)
			fshad
				<< "uniform vec4 pxl_n_half_pxl_size;\n"
				<< GenerateGaussFunctionCode(
					(caps & gauss_krn_diameter_mask) >> 16,
					true
					)
				;

		fshad <<
			"\n"
			"void main() {\n";

		GNUVG_ERROR("caps: %x -- gradient_spread_mask: %x -- %x\n",
			    caps, gradient_spread_mask,
			    caps & gradient_spread_mask);

		if(caps & do_mask)
			fshad <<
				"  vec4 m = texture( m_texture, v_maskCoord );\n";

		switch(primary_mode) {
		case do_flat_color:
			fshad << "  vec4 c = v_color;\n";
			break;

		case do_linear_gradient:
		case do_radial_gradient:
			if(primary_mode == do_linear_gradient)
				fshad <<
					"  vec2 BA = gradient_coord - linear_start;\n"
					"  float g = ((BA.x*linear_normal.y) - (BA.y*linear_normal.x)) / linear_length;\n"
					;
			else
				fshad <<
					"  vec2 dxy = coord - radial.xy;\n"
					"  float g = dot(dxy, radial.zw);\n"
					"  float h = dxy.x * radial.w - dxy.y * radial.z;\n"
					"  g += sqrt(radius2 * dot(dxy, dxy) - h*h);\n"
					"  g *= radial_denom;\n"
					;

			switch(caps & gradient_spread_mask) {
			case do_gradient_pad:
				fshad << "    g = clamp(g, 0.0, 1.0);\n";
				break;
			case do_gradient_repeat:
				fshad << "    g = mod(g, 1.0);\n";
				break;
			case do_gradient_reflect:
				fshad <<
					"    float uneven;\n"
					"    g = mod(g, 1.0);\n"
					"    uneven = floor(0.6 + mod(abs(floor(g))/2.0, 1.0));\n"
					"    g = g * (1.0 - uneven) + (1.0 - g) * uneven;\n";
				break;
			}

			fshad <<
				"  vec4 previous_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);\n"
				"  vec4 c = stop_colors[nr_stops - 1];\n"
				"  for(int i = 0; i < nr_stops; i++) {\n"
				"    if(g < stop_offsets[i]) {\n"
				"      float coef = (stop_offsets[i] - g) * stop_invfactor[i];\n"
				"      c = mix(stop_colors[i], previous_color, coef);\n"
				"    }\n"
				"    previous_color = stop_colors[i];\n"
				"  }\n";
			break;

		case do_pattern:
			if(do_gauss) {
				auto hpx_size = horizontal_gauss ?
					"pxl_n_half_pxl_size.z, 0" :
					"0, pxl_n_half_pxl_size.w"
					;
				auto px_size = horizontal_gauss ?
					"pxl_n_half_pxl_size.x, 0" :
					"0, pxl_n_half_pxl_size.y"
					;
				fshad <<
					"  vec4 c = GaussianBlur( p_texture, p_textureCoord.xy, \n"
					"                    vec2( " << hpx_size << " ),\n"
					"                    vec2( " <<  px_size << " ) );\n"
					;
			} else {
				fshad <<
					"  vec4 c = texture( p_texture, p_textureCoord.xy );\n";
			}
			break;

		case do_texture:
			fshad <<
				"  vec4 c = texture( u_textureSampler, v_textureCoord.xy );\n";
			break;
		}

		if(caps & do_mask)
			fshad << "  c = m.a * c;\n";

		if(caps & do_color_transform)
			fshad <<
				"  o_color.r = c.r * ctransform_scale.r + ctransform_bias.r;\n"
				"  o_color.g = c.g * ctransform_scale.g + ctransform_bias.g;\n"
				"  o_color.b = c.b * ctransform_scale.b + ctransform_bias.b;\n"
				"  o_color.a = c.a * ctransform_scale.a + ctransform_bias.a;\n"
				;
		else
			fshad <<
				"  o_color = c;\n"
				;

		fshad <<
			"}\n"
			;

		return fshad.str();
	}

	eka2l1::drivers::handle Shader::create_program(eka2l1::drivers::graphics_driver *drv, const char *vertexshader_source, const char *fragmentshader_source,
		eka2l1::drivers::shader_program_metadata *metadata) {
		std::string compile_log;
		eka2l1::drivers::handle vertex_module = eka2l1::drivers::create_shader_module(drv,
			vertexshader_source, strlen(vertexshader_source), eka2l1::drivers::shader_module_type::vertex,
			&compile_log);

		if (!vertex_module) {
			GNUVG_ERROR("Vertex shader failed to compile with error: {}", compile_log);
			return 0;
		}

		eka2l1::drivers::handle fragment_module = eka2l1::drivers::create_shader_module(drv,
			fragmentshader_source, strlen(fragmentshader_source), eka2l1::drivers::shader_module_type::fragment,
			&compile_log);

		if (!vertex_module) {
			GNUVG_ERROR("Fragment shader failed to compile with error: {}", compile_log);
			return 0;
		}

		eka2l1::drivers::handle program_handle = eka2l1::drivers::create_shader_program(drv, vertex_module, fragment_module, metadata, &compile_log);
		if (!program_handle) {
			GNUVG_ERROR("Program failed to link with error: {}", compile_log);
			return 0;
		}

		eka2l1::drivers::graphics_command_builder builder;
		builder.destroy(vertex_module);
		builder.destroy(fragment_module);

		eka2l1::drivers::command_list final_list = builder.retrieve_command_list();
		drv->submit_command_list(final_list);

		return program_handle;
	}

	static void print_shader(const std::string &title, const std::string &content_s) {
		GNUVG_ERROR("%s\n", title.c_str());

		auto content = content_s.c_str();
		std::vector<char> bfr;
		size_t k = 0, l = 0;

		while(content[k] != '\0') {
			bfr.clear();
			while(content[k] != '\n' && content[k] != '\0') {
				bfr.push_back(content[k]);
				k += 1;
			}
			bfr.push_back('\0');
			GNUVG_ERROR("%lx: %s\n", l, bfr.data());
			if(content[k] == '\n') k += 1;
			l++;
		}
	}

	Shader::Shader(eka2l1::drivers::graphics_driver *drv, int caps) {
		auto vertex_shader = build_vertex_shader(caps);
		auto fragment_shader = build_fragment_shader(caps);

		if(caps & do_horizontal_gaussian || caps & do_vertical_gaussian)
			GNUVG_ERROR("Caps debug, gauss: %x\n", caps);

		print_shader("Vertex Shader:", vertex_shader);
		print_shader("Fragment Shader:", fragment_shader);

		std::string version = "";

		if (drv->is_stricted()) {
			version = "#version 300 es\n";
		} else {
			version = "#version 140\n#extension GL_ARB_explicit_attrib_location : require\n";
		}

		vertex_shader.insert(0, version);
		fragment_shader.insert(0, version);

		eka2l1::drivers::shader_program_metadata metadata;
		program_id = create_program(drv, vertex_shader.c_str(), fragment_shader.c_str(),
			&metadata);

		textureMatrix_handle = metadata.get_uniform_binding("u_textureMatrix");
		textureSampler_handle = metadata.get_uniform_binding("u_textureSampler");

		ColorHandle = metadata.get_uniform_binding("v_color");
		Matrix = metadata.get_uniform_binding("modelview_projection");
		preTranslation = metadata.get_uniform_binding("pre_translation");

		maskTexture = metadata.get_uniform_binding("m_texture" );

		pxl_sizes = metadata.get_uniform_binding("pxl_n_half_pxl_size");
		patternTexture = metadata.get_uniform_binding("p_texture" );
		patternMatrix = metadata.get_uniform_binding("p_projection");

		surf2paint = metadata.get_uniform_binding("surf2paint");

		ctransform_scale = metadata.get_uniform_binding("ctransform_scale");
		ctransform_bias = metadata.get_uniform_binding("ctransform_bias");

		linear_normal = metadata.get_uniform_binding("linear_normal");
		linear_start = metadata.get_uniform_binding("linear_start");
		linear_length = metadata.get_uniform_binding("linear_length");

		radial = metadata.get_uniform_binding("radial");
		radius2 = metadata.get_uniform_binding("radius2");
		radial_denom = metadata.get_uniform_binding("radial_denom");

		stop_colors = metadata.get_uniform_binding("stop_colors");
		stop_invfactor = metadata.get_uniform_binding("stop_invfactor");
		stop_offsets = metadata.get_uniform_binding("stop_offsets");
		nr_stops = metadata.get_uniform_binding("nr_stops");
	}
};
